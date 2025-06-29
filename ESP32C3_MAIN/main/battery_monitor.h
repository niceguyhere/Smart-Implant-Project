#pragma once

void battery_init(void);
float read_filtered_voltage(void);
int estimate_soc(float voltage);
