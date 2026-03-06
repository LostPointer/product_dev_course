#!/usr/bin/env python3
"""Fix webhooks_dispatcher test mocks"""
import re

FILE_PATH = '/home/lostpointer/product_dev_course/projects/backend/services/experiment-service/tests/test_webhooks_dispatcher.py'

with open(FILE_PATH, 'r') as f:
    content = f.read()

# Pattern: Replace old mock setup with proper async context manager
old_pattern = r'session\.post\.return_value\.__aenter__\.return_value = response'
new_pattern = '''mock_cm = MagicMock()
        mock_cm.__aenter__ = AsyncMock(return_value=response)
        mock_cm.__aexit__ = AsyncMock(return_value=None)
        session.post.return_value = mock_cm'''

content = re.sub(old_pattern, new_pattern, content)

with open(FILE_PATH, 'w') as f:
    f.write(content)

print("✅ Fixed webhooks_dispatcher test mocks")
