FROM python:3.11-slim

ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends libglib2.0-0 libgl1 \
    && rm -rf /var/lib/apt/lists/*
COPY pyproject.toml README.md ./
COPY src ./src
COPY configs ./configs
COPY dashboard.py ./
RUN pip install --no-cache-dir ".[dashboard]"

EXPOSE 8501
CMD ["streamlit", "run", "dashboard.py", "--server.address=0.0.0.0"]

