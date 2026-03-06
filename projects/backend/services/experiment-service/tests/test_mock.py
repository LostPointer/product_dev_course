#!/usr/bin/env python3
from unittest.mock import AsyncMock, MagicMock
import asyncio

async def test():
    session = MagicMock()  # Не AsyncMock!
    response = AsyncMock()
    response.status = 200
    response.text = AsyncMock(return_value="")
    mock_cm = MagicMock()
    mock_cm.__aenter__ = AsyncMock(return_value=response)
    mock_cm.__aexit__ = AsyncMock(return_value=None)
    session.post.return_value = mock_cm
    
    print("session.post:", session.post)
    print("session.post.return_value:", session.post.return_value)
    
    async with session.post("http://test.com") as resp:
        print("resp:", resp)
        print("resp.status:", resp.status)
        print("OK!")

asyncio.run(test())
