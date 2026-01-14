"""Shared aiohttp application helpers."""
from __future__ import annotations

from pathlib import Path
from typing import Literal, Protocol

from aiohttp import web
from aiohttp_cors import CorsConfig, ResourceOptions, setup as cors_setup

from backend_common.middleware.trace import create_trace_middleware


class SettingsProtocol(Protocol):
    """Protocol for settings objects used by the app helpers."""

    app_name: str
    env: Literal["development", "staging", "production"]
    cors_allowed_origins: list[str]


def create_base_app(settings: SettingsProtocol) -> tuple[web.Application, CorsConfig]:
    """Create a base aiohttp app with tracing middleware and CORS configured."""
    app = web.Application()

    trace_middleware = create_trace_middleware(settings.app_name)
    app.middlewares.append(trace_middleware)

    cors = cors_setup(
        app,
        defaults={
            origin: ResourceOptions(
                allow_credentials=True,
                expose_headers="*",
                allow_headers="*",
                allow_methods="*",
            )
            for origin in settings.cors_allowed_origins
        },
    )

    return app, cors


def add_healthcheck(app: web.Application, settings: SettingsProtocol) -> None:
    """Register a standard health check endpoint."""

    async def healthcheck(_request: web.Request) -> web.Response:
        return web.json_response({"status": "ok", "service": settings.app_name, "env": settings.env})

    app.router.add_get("/health", healthcheck)


def add_openapi_spec(app: web.Application, openapi_path: Path) -> None:
    """Register an endpoint that serves the OpenAPI spec."""

    async def openapi_spec(_request: web.Request) -> web.StreamResponse:
        return web.FileResponse(openapi_path, headers={"Content-Type": "application/yaml"})

    app.router.add_get("/openapi.yaml", openapi_spec)


def add_cors_to_routes(app: web.Application, cors: CorsConfig) -> None:
    """Apply CORS configuration to all routes in the app."""
    for route in list(app.router.routes()):
        cors.add(route)
