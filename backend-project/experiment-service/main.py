"""Experiment Service - сервис для управления экспериментами."""
import asyncio
import logging
from aiohttp import web
from aiohttp.web_middlewares import normalize_path_middleware

from src.config import settings
from src.database import init_db, close_db
from src.handlers import experiments, runs
from src.middleware import auth_middleware, error_middleware

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def create_app() -> web.Application:
    """Создание aiohttp приложения."""
    app = web.Application(
        middlewares=[
            normalize_path_middleware(),
            error_middleware,
            auth_middleware,
        ]
    )

    # Регистрация routes
    app.router.add_get('/health', health_handler)

    # Experiments endpoints
    app.router.add_get('/experiments', experiments.list_experiments)
    app.router.add_post('/experiments', experiments.create_experiment)
    app.router.add_get('/experiments/{experiment_id}', experiments.get_experiment)
    app.router.add_put('/experiments/{experiment_id}', experiments.update_experiment)
    app.router.add_delete('/experiments/{experiment_id}', experiments.delete_experiment)
    app.router.add_get('/experiments/search', experiments.search_experiments)

    # Runs endpoints
    app.router.add_get('/experiments/{experiment_id}/runs', runs.list_runs)
    app.router.add_post('/experiments/{experiment_id}/runs', runs.create_run)
    app.router.add_get('/runs/{run_id}', runs.get_run)
    app.router.add_put('/runs/{run_id}', runs.update_run)
    app.router.add_put('/runs/{run_id}/complete', runs.complete_run)
    app.router.add_put('/runs/{run_id}/fail', runs.fail_run)

    # Startup и cleanup
    app.on_startup.append(on_startup)
    app.on_cleanup.append(on_cleanup)

    return app


async def health_handler(request: web.Request) -> web.Response:
    """Health check endpoint."""
    return web.json_response({
        "status": "healthy",
        "service": "experiment-service",
        "version": "1.0.0"
    })


async def on_startup(app: web.Application) -> None:
    """Инициализация при старте."""
    logger.info("Starting Experiment Service...")
    await init_db()
    logger.info("Experiment Service started successfully")


async def on_cleanup(app: web.Application) -> None:
    """Очистка при остановке."""
    logger.info("Shutting down Experiment Service...")
    await close_db()
    logger.info("Experiment Service stopped")


def main():
    """Точка входа."""
    app = create_app()
    web.run_app(
        app,
        host=settings.HOST,
        port=settings.PORT,
        access_log=logger
    )


if __name__ == '__main__':
    main()

