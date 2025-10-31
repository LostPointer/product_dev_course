"""Публикация событий в RabbitMQ."""
import json
import logging
from typing import Dict, Any

from src.config import settings

logger = logging.getLogger(__name__)

# TODO: Реализовать подключение к RabbitMQ
# Для заготовки просто логируем события


async def publish_event(event_type: str, event_data: Dict[str, Any]) -> None:
    """Публикация события в RabbitMQ."""
    try:
        # TODO: Реальная публикация через aio-pika
        # from aio_pika import connect_robust, Message
        # connection = await connect_robust(settings.RABBITMQ_URL)
        # channel = await connection.channel()
        # exchange = await channel.declare_exchange(settings.RABBITMQ_EXCHANGE, ExchangeType.TOPIC)
        #
        # message = Message(
        #     json.dumps(event_data).encode(),
        #     content_type='application/json'
        # )
        # await exchange.publish(message, routing_key=event_type)

        # Временное логирование
        logger.info(f"Event published: {event_type}", extra={"event_data": event_data})

    except Exception as e:
        logger.error(f"Failed to publish event {event_type}: {e}")

