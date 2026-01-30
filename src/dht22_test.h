#pragma once 

// Lanza una nueva tarea que lee el DHT22 periódicamente y saca logs
// Útil para validar cableado + driver antes de integrarlo en sensors.c/FSM
void dht22_test_start(void);