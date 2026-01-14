"""aiohttp application entrypoint."""
from __future__ import annotations

from pathlib import Path
from typing import Any

from aiohttp import web

from backend_common.aiohttp_app import (
    add_cors_to_routes,
    add_healthcheck,
    add_openapi_spec,
    create_base_app,
)
from backend_common.db.pool import close_pool_service as close_pool, init_pool_service
from backend_common.logging_config import configure_logging

from telemetry_ingest_service.api.routes.telemetry import routes as telemetry_routes
from telemetry_ingest_service.settings import settings

# Configure structured logging
configure_logging()

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
OPENAPI_PATH = PROJECT_ROOT / "openapi" / "openapi.yaml"


async def init_pool(_app: Any = None) -> None:
    await init_pool_service(_app, settings)


def create_app() -> web.Application:
    app, cors = create_base_app(settings)

    add_healthcheck(app, settings)
    add_openapi_spec(app, OPENAPI_PATH)
    app.add_routes(telemetry_routes)

    app.on_startup.append(init_pool)
    app.on_cleanup.append(close_pool)

    add_cors_to_routes(app, cors)

    return app


def main() -> None:
    web.run_app(create_app(), host=settings.host, port=settings.port, access_log=None)


if __name__ == "__main__":
    main()

