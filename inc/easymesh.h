#ifndef __EASYMESH_H
#define __EASYMESH_H

#define __FRAME_DATA 0x20
#define __FRAME_NETWORK 0x40
#define __TTL 5

typedef struct {
	uint8_t data[32];
} nrf_frame_t;

uint8_t easymesh_init();
uint8_t while_true_func();
uint8_t encapsulate(uint8_t* data, uint16_t length);

#endif /* __EASYMESH_H */
