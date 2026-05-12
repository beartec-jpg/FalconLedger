# Copyright (c) 2026 qXRP Team. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

FROM python:3.13-slim

WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY server.py .

ENV XRPLD_RPC_URL=http://localhost:5005
ENV DASHBOARD_PORT=8080

EXPOSE 8080

CMD ["python3", "server.py"]
