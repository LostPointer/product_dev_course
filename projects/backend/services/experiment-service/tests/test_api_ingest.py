from __future__ import annotations

# pyright: reportMissingImports=false

import uuid

import pytest

from experiment_service.services.idempotency import IDEMPOTENCY_HEADER
from tests.utils import make_headers


async def _bootstrap_ingest_context(service_client, *, sensor_payload_overrides: dict | None = None):
    project_id = uuid.uuid4()
    headers = make_headers(project_id)

    resp = await service_client.post(
        "/api/v1/experiments",
        json={"project_id": str(project_id), "name": "Telemetry experiment"},
        headers=headers,
    )
    assert resp.status == 201
    experiment_id = (await resp.json())["id"]

    resp_run = await service_client.post(
        f"/api/v1/experiments/{experiment_id}/runs",
        json={"name": "ingest-run"},
        headers=headers,
    )
    assert resp_run.status == 201
    run = await resp_run.json()
    run_id = run["id"]

    resp_session = await service_client.post(
        f"/api/v1/runs/{run_id}/capture-sessions",
        json={"ordinal_number": 1, "status": "running"},
        headers=headers,
    )
    assert resp_session.status == 201
    capture_session_id = (await resp_session.json())["id"]

    sensor_payload: dict = {
        "project_id": str(project_id),
        "name": "sensor-ingest",
        "type": "thermocouple",
        "input_unit": "mV",
        "display_unit": "C",
    }
    if sensor_payload_overrides:
        sensor_payload.update(sensor_payload_overrides)
    resp_sensor = await service_client.post(
        "/api/v1/sensors",
        json=sensor_payload,
        headers={**headers, IDEMPOTENCY_HEADER: "ingest-sensor"},
    )
    assert resp_sensor.status == 201
    sensor_body = await resp_sensor.json()
    sensor_id = sensor_body["sensor"]["id"]
    token = sensor_body["token"]

    return {
        "project_id": project_id,
        "headers": headers,
        "sensor_id": sensor_id,
        "token": token,
        "run_id": run_id,
        "capture_session_id": capture_session_id,
        "experiment_id": experiment_id,
    }


@pytest.mark.asyncio
async def test_metrics_ingest_accepts_payload(service_client):
    ctx = await _bootstrap_ingest_context(service_client)
    run_id = ctx["run_id"]
    headers = ctx["headers"]
    resp = await service_client.post(
        f"/api/v1/runs/{run_id}/metrics",
        json={
            "metrics": [
                {"name": "loss", "step": 1, "value": 0.1, "timestamp": "2025-01-01T00:00:00Z"}
            ]
        },
        headers=headers,
    )
    assert resp.status == 202
    body = await resp.json()
    assert body["accepted"] == 1


@pytest.mark.asyncio
async def test_metrics_query_returns_series(service_client):
    ctx = await _bootstrap_ingest_context(service_client)
    headers = ctx["headers"]
    run_id = ctx["run_id"]
    payload = {
        "metrics": [
            {"name": "loss", "step": 1, "value": 0.1, "timestamp": "2025-01-01T00:00:00Z"}
        ]
    }
    resp = await service_client.post(
        f"/api/v1/runs/{run_id}/metrics",
        json=payload,
        headers=headers,
    )
    assert resp.status == 202

    resp = await service_client.get(
        f"/api/v1/runs/{run_id}/metrics",
        headers=headers,
    )
    assert resp.status == 200
    data = await resp.json()
    assert data["run_id"] == str(run_id)
    assert data["series"][0]["points"][0]["value"] == 0.1

