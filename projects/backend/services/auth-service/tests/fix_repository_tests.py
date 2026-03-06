#!/usr/bin/env python3
"""Script to fix mock patterns in test_repositories.py"""
import re

FILE_PATH = '/home/lostpointer/product_dev_course/projects/backend/services/auth-service/tests/test_repositories.py'

with open(FILE_PATH, 'r') as f:
    content = f.read()

# Pattern 1: Replace old mock setup with fixture
old_pattern = r'mock_pool = AsyncMock\(\)\s+mock_conn = AsyncMock\(\)\s+mock_pool\.acquire\.return_value\.__aenter__\.return_value = mock_conn'
new_pattern = 'mock_pool, mock_conn = mock_pool_with_conn'

content = re.sub(old_pattern, new_pattern, content)

# Pattern 2: Add fixture parameter to test methods that need it
# Find all async test methods in repository test classes that don't have the fixture
test_classes = [
    'TestUserRepositoryCreate',
    'TestUserRepositoryGetters', 
    'TestUserRepositoryUpdate',
    'TestUserRepositoryQueries',
    'TestProjectRepositoryCreate',
    'TestProjectRepositoryGetters',
    'TestProjectRepositoryUpdate',
    'TestProjectRepositoryMembers',
    'TestProjectRepositoryDelete',
]

for class_name in test_classes:
    # Find the class and add fixture to all async test methods
    class_pattern = rf'(class {class_name}:.*?)(?=\nclass |\Z)'
    
    def add_fixture_to_methods(match):
        class_content = match.group(1)
        # Add fixture parameter to async test methods
        method_pattern = r'(    @pytest\.mark\.asyncio\s+async def test_\w+)\(self\):'
        class_content = re.sub(method_pattern, r'\1(self, mock_pool_with_conn):', class_content)
        return class_content
    
    content = re.sub(class_pattern, add_fixture_to_methods, content, flags=re.DOTALL)

with open(FILE_PATH, 'w') as f:
    f.write(content)

print("✅ Fixed test_repositories.py")
print("   - Replaced mock patterns with fixture")
print("   - Added mock_pool_with_conn parameter to test methods")
