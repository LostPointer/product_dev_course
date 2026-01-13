from __future__ import annotations

import asyncio
import hmac
import json
import uuid
from hashlib import sha256

import pytest
from aiohttp import web

from tests.utils import make_headers


@pytest.mark.asyncio
async def test_webhook_delivery_for_capture_session_created(service_client):
    project_id = uuid.uuid4()
    headers_owner = make_headers(project_id, role="owner")

    received: asyncio.Queue[tuple[dict[str, str], dict]] = asyncio.Queue()

    async def handler(request: web.Request) -> web.Response:
        raw = await request.read()
        body = json.loads(raw.decode("utf-8"))
        headers = {k: v for k, v in request.headers.items()}
        await received.put((headers, body))
        return web.Response(status=200)

    app = web.Application()
    app.router.add_post("/hook", handler)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, "127.0.0.1", 0)
    await site.start()
    port = site._server.sockets[0].getsockname()[1]  # type: ignore[union-attr]
    target_url = f"http://127.0.0.1:{port}/hook"

    try:
        secret = "test-secret"

        # Create webhook subscription
        resp = await service_client.post(
            "/api/v1/webhooks",
            json={
                "target_url": target_url,
                "event_types": ["capture_session.created"],
                "secret": secret,
            },
            headers=headers_owner,
        )
        assert resp.status == 201

        # Create experiment + run
        resp = await service_client.post(
            "/api/v1/experiments",
            json={"project_id": str(project_id), "name": "Webhook experiment"},
            headers=headers_owner,
        )
        assert resp.status == 201
        experiment_id = (await resp.json())["id"]

        resp = await service_client.post(
            f"/api/v1/experiments/{experiment_id}/runs",
            json={"name": "Webhook run"},
            headers=headers_owner,
        )
        assert resp.status == 201
        run_id = (await resp.json())["id"]

        # Trigger capture_session.created
        resp = await service_client.post(
            f"/api/v1/runs/{run_id}/capture-sessions",
            json={"ordinal_number": 1, "status": "running", "notes": "hello"},
            headers=headers_owner,
        )
        assert resp.status == 201

        headers, body = await asyncio.wait_for(received.get(), timeout=3.0)
        assert headers["X-Webhook-Event"] == "capture_session.created"
        assert "X-Webhook-Delivery-Id" in headers
        assert body["event_type"] == "capture_session.created"
        assert body["project_id"] == str(project_id)
        assert body["payload"]["run_id"] == str(run_id)
        assert body["payload"]["notes"] == "hello"

        # Validate signature (same canonicalization as dispatcher)
        body_bytes = json.dumps(body, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        expected = "sha256=" + hmac.new(secret.encode("utf-8"), body_bytes, sha256).hexdigest()
        assert headers["X-Webhook-Signature"] == expected
    finally:
        await runner.cleanup()


@pytest.mark.asyncio
async def test_webhook_subscription_requires_owner_or_editor(service_client):
    project_id = uuid.uuid4()
    headers_viewer = make_headers(project_id, role="viewer")

    resp = await service_client.post(
        "/api/v1/webhooks",
        json={"target_url": "http://example.com/hook", "event_types": ["run.started"]},
        headers=headers_viewer,
    )
    assert resp.status in (403, 404)

