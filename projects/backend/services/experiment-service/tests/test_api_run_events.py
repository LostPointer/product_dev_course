from __future__ import annotations

import uuid

import pytest

from tests.utils import make_headers


@pytest.mark.asyncio
async def test_run_events_recorded_for_status_change_and_bulk_tags(service_client):
    project_id = uuid.uuid4()
    headers_owner = make_headers(project_id, role="owner")
    headers_viewer = make_headers(project_id, role="viewer")

    # Create experiment
    resp = await service_client.post(
        "/api/v1/experiments",
        json={"project_id": str(project_id), "name": "Run events"},
        headers=headers_owner,
    )
    assert resp.status == 201
    experiment_id = (await resp.json())["id"]

    # Create run
    resp = await service_client.post(
        f"/api/v1/experiments/{experiment_id}/runs",
        json={"name": "Run for events"},
        headers=headers_owner,
    )
    assert resp.status == 201
    run_id = (await resp.json())["id"]

    # status change -> running
    resp = await service_client.patch(
        f"/api/v1/runs/{run_id}",
        json={"status": "running"},
        headers=headers_owner,
    )
    assert resp.status == 200

    # bulk tags add
    resp = await service_client.post(
        "/api/v1/runs:bulk-tags",
        json={"run_ids": [run_id], "add_tags": ["alpha", "beta"]},
        headers=headers_owner,
    )
    assert resp.status == 200

    # viewer can read events
    resp = await service_client.get(
        f"/api/v1/runs/{run_id}/events",
        headers=headers_viewer,
    )
    assert resp.status == 200
    body = await resp.json()
    assert body["total"] >= 2
    types = [evt["event_type"] for evt in body["events"]]
    assert "run.status_changed" in types
    assert "run.tags_updated" in types

