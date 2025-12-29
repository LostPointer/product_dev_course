-- 002_sensor_projects_many_to_many.sql
-- Migration to support many-to-many relationship between sensors and projects.
-- This allows a sensor to be associated with multiple projects.

BEGIN;

-- Step 1: Create the sensor_projects junction table
CREATE TABLE IF NOT EXISTS sensor_projects (
    sensor_id uuid NOT NULL,
    project_id uuid NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (sensor_id, project_id),
    FOREIGN KEY (sensor_id) REFERENCES sensors (id) ON DELETE CASCADE,
    -- Note: project_id references auth_service.projects, but we can't add FK here
    -- as it's in a different database. We rely on application-level validation.
    UNIQUE (sensor_id, project_id)
);

-- Create indexes only if they don't exist
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_indexes WHERE indexname = 'sensor_projects_project_idx') THEN
        CREATE INDEX sensor_projects_project_idx ON sensor_projects (project_id);
    END IF;
    IF NOT EXISTS (SELECT 1 FROM pg_indexes WHERE indexname = 'sensor_projects_sensor_idx') THEN
        CREATE INDEX sensor_projects_sensor_idx ON sensor_projects (sensor_id);
    END IF;
END $$;

-- Step 2: Migrate existing data from sensors.project_id to sensor_projects
-- For each sensor, create a corresponding entry in sensor_projects
INSERT INTO sensor_projects (sensor_id, project_id, created_at)
SELECT id, project_id, created_at
FROM sensors
ON CONFLICT (sensor_id, project_id) DO NOTHING;

-- Step 3: We keep sensors.project_id as NOT NULL for now to maintain backward compatibility
-- with existing foreign keys. In a future migration, we could make it nullable or remove it
-- after updating all dependent tables.

-- Step 4: Add a comment explaining the dual structure during migration
COMMENT ON TABLE sensor_projects IS 'Many-to-many relationship between sensors and projects. Each sensor can belong to multiple projects.';
COMMENT ON COLUMN sensors.project_id IS 'Primary project for backward compatibility. All projects are tracked in sensor_projects table.';

COMMIT;

