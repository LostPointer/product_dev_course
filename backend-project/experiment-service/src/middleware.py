"""Middleware для приложения."""
import logging
import json
from aiohttp import web
from aiohttp.web_request import Request

from src.config import settings

logger = logging.getLogger(__name__)


async def auth_middleware(request: Request, handler):
    """Middleware для проверки аутентификации."""
    # Публичные endpoints
    public_paths = ['/health']

    if request.path in public_paths:
        return await handler(request)

    # Проверка токена
    auth_header = request.headers.get('Authorization')
    if not auth_header or not auth_header.startswith('Bearer '):
        raise web.HTTPUnauthorized(text="Missing or invalid Authorization header")

    token = auth_header.replace('Bearer ', '')

    # Валидация токена через Auth Service
    # Для заготовки просто извлекаем user_id из токена (упрощенная версия)
    # В продакшене нужно валидировать через Auth Service
    try:
        # TODO: Реальная валидация через Auth Service
        # async with httpx.AsyncClient() as client:
        #     response = await client.get(
        #         f"{settings.AUTH_SERVICE_URL}/verify",
        #         headers={"Authorization": f"Bearer {token}"}
        #     )
        #     user_data = response.json()
        #     request['user_id'] = UUID(user_data['user_id'])

        # Временная заглушка - извлекаем user_id из заголовка или токена
        # В реальной реализации нужно валидировать через Auth Service
        request['user_id'] = None  # Будет установлено после валидации

    except Exception as e:
        logger.error(f"Auth validation failed: {e}")
        raise web.HTTPUnauthorized(text="Invalid token")

    return await handler(request)


async def error_middleware(request: Request, handler):
    """Middleware для обработки ошибок."""
    try:
        response = await handler(request)
        return response
    except web.HTTPException:
        raise
    except Exception as e:
        logger.error(f"Unhandled error: {e}", exc_info=True)
        return web.json_response(
            {"error": "Internal server error"},
            status=500
        )

