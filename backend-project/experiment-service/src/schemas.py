"""Pydantic схемы для валидации."""
from datetime import datetime
from typing import Optional, List, Dict, Any
from uuid import UUID

from pydantic import BaseModel, Field, validator


class ExperimentBase(BaseModel):
    """Базовая схема эксперимента."""
    name: str = Field(..., min_length=1, max_length=255)
    description: Optional[str] = None
    experiment_type: Optional[str] = Field(None, max_length=100)  # aerodynamics, strength, etc.
    tags: Optional[List[str]] = []
    metadata: Optional[Dict[str, Any]] = {}


class ExperimentCreate(ExperimentBase):
    """Схема создания эксперимента."""
    project_id: UUID


class ExperimentUpdate(BaseModel):
    """Схема обновления эксперимента."""
    name: Optional[str] = Field(None, min_length=1, max_length=255)
    description: Optional[str] = None
    experiment_type: Optional[str] = Field(None, max_length=100)
    tags: Optional[List[str]] = None
    metadata: Optional[Dict[str, Any]] = None
    status: Optional[str] = None

    @validator('status')
    def validate_status(cls, v):
        if v and v not in ['created', 'running', 'completed', 'failed', 'archived']:
            raise ValueError('Invalid status')
        return v


class ExperimentResponse(ExperimentBase):
    """Схема ответа с эксперимента."""
    id: UUID
    project_id: UUID
    created_by: UUID
    status: str
    created_at: datetime
    updated_at: datetime

    class Config:
        from_attributes = False  # для работы с dict из asyncpg


class RunBase(BaseModel):
    """Базовая схема run."""
    name: str = Field(..., min_length=1, max_length=255)
    parameters: Dict[str, Any] = Field(..., description="Параметры запуска эксперимента")
    notes: Optional[str] = None
    metadata: Optional[Dict[str, Any]] = {}


class RunCreate(RunBase):
    """Схема создания run."""
    pass


class RunUpdate(BaseModel):
    """Схема обновления run."""
    name: Optional[str] = Field(None, min_length=1, max_length=255)
    parameters: Optional[Dict[str, Any]] = None
    notes: Optional[str] = None
    metadata: Optional[Dict[str, Any]] = None
    status: Optional[str] = None

    @validator('status')
    def validate_status(cls, v):
        if v and v not in ['created', 'running', 'completed', 'failed']:
            raise ValueError('Invalid status')
        return v


class RunResponse(RunBase):
    """Схема ответа с run."""
    id: UUID
    experiment_id: UUID
    status: str
    started_at: Optional[datetime] = None
    completed_at: Optional[datetime] = None
    duration_seconds: Optional[int] = None
    created_at: datetime
    updated_at: datetime

    class Config:
        from_attributes = False


class ExperimentListResponse(BaseModel):
    """Схема списка экспериментов."""
    experiments: List[ExperimentResponse]
    total: int
    page: int
    page_size: int


class RunListResponse(BaseModel):
    """Схема списка runs."""
    runs: List[RunResponse]
    total: int
    page: int
    page_size: int

