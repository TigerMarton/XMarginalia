FROM python:3.13-slim

COPY requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r /app/requirements.txt

ENV PYTHONUNBUFFERED=1
WORKDIR /app
COPY companion /app/companion

# Non-root uid 10004 (10001=getbooks, 10002=ankibridge, 10003=readbridge)
RUN useradd --uid 10004 --no-create-home --shell /usr/sbin/nologin companion \
 && mkdir -p /data /abs_books && chown -R 10004:10004 /data /app
USER 10004:10004

EXPOSE 8095
CMD ["uvicorn", "companion.app:app", "--host", "0.0.0.0", "--port", "8095", "--timeout-keep-alive", "75"]
