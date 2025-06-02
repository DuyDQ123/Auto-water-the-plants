-- Create database
CREATE DATABASE IF NOT EXISTS iot_control;

-- Use database
USE iot_control;

-- Create pump events table
CREATE TABLE IF NOT EXISTS pump_events (
    id INT AUTO_INCREMENT PRIMARY KEY,
    state BOOLEAN NOT NULL,
    timestamp DATETIME NOT NULL
);