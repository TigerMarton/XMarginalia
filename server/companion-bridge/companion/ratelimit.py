"""Rate limiting and failure-driven lockout for the Companion API."""

import time


class Window:
    """Sliding-window counter keyed by client IP."""

    SWEEP_EVERY = 256

    def __init__(self, limit: int, per_s: float):
        self.limit = limit
        self.per_s = per_s
        self.hits: dict[str, list[float]] = {}
        self._calls_since_sweep = 0

    def _sweep(self, now: float) -> None:
        cutoff = now - self.per_s
        self.hits = {k: v for k, v in self.hits.items() if v and v[-1] > cutoff}

    def allow(self, key: str, now: float | None = None) -> bool:
        now = time.time() if now is None else now
        self._calls_since_sweep += 1
        if self._calls_since_sweep >= self.SWEEP_EVERY:
            self._calls_since_sweep = 0
            self._sweep(now)
        hits = [t for t in self.hits.get(key, []) if t > now - self.per_s]
        if len(hits) >= self.limit:
            self.hits[key] = hits
            return False
        hits.append(now)
        self.hits[key] = hits
        return True


class Lockout:
    """Exponential per-IP backoff on invalid X-API-Key attempts."""

    FREE_FAILURES = 2
    BASE_S = 30.0
    MAX_S = 3600.0
    FORGET_S = 86400.0
    SWEEP_EVERY = 256

    def __init__(self):
        self._state: dict[str, list[ float | int ]] = {}
        self._calls_since_sweep = 0

    def _sweep(self, now: float) -> None:
        self._state = {k: v for k, v in self._state.items() if now - v[1] < self.FORGET_S}

    def _penalty(self, failures: int) -> float:
        charged = failures - self.FREE_FAILURES
        if charged <= 0:
            return 0.0
        return min(self.MAX_S, self.BASE_S * (2 ** (charged - 1)))

    def locked_for(self, key: str, now: float | None = None) -> float:
        now = time.time() if now is None else now
        entry = self._state.get(key)
        if not entry:
            return 0.0
        failures, last = int(entry[0]), float(entry[1])
        if now - last >= self.FORGET_S:
            return 0.0
        remaining = self._penalty(failures) - (now - last)
        return remaining if remaining > 0 else 0.0

    def record_failure(self, key: str, now: float | None = None) -> None:
        now = time.time() if now is None else now
        self._calls_since_sweep += 1
        if self._calls_since_sweep >= self.SWEEP_EVERY:
            self._calls_since_sweep = 0
            self._sweep(now)
        entry = self._state.get(key)
        failures = int(entry[0]) + 1 if entry and now - float(entry[1]) < self.FORGET_S else 1
        self._state[key] = [failures, now]

    def record_success(self, key: str) -> None:
        self._state.pop(key, None)
