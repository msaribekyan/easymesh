#include "main.h"
#include "usbd_cdc_if.h"
#include "NRF24L01.h"
#include "string.h"

#include "easymesh.h"

/* SETUP VARIABLES BEGIN*/
uint8_t node_address = 0;
uint8_t common_channel = 0;

uint8_t RxAddress[] = {0x00,0xDD,0xCC,0xBB,0xAA};
uint8_t TxAddress[] = {0xEE,0xDD,0xCC,0xBB,0xAA};
/* SETUP VARIABLES END*/

nrf_mode_t current_mode = __MODE_RX;
uint8_t seq_id = 0;
uint8_t members[255] = {255}; //Store the last received Sequence ID of that node

uint8_t easymesh_init(uint8_t channel, uint8_t address){
	common_channel = channel;
	node_address = address;
	NRF24_Init();
	NRF24_TxMode(TxAddress, common_channel);
	HAL_Delay(10);
	NRF24_RxMode(RxAddress, common_channel);
	memset(members, 255, 255);
	HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, SET);
	return 0;
}

nrf_tx_queue_node_t* nrf_tx_queue_head = NULL;
nrf_tx_queue_node_t* nrf_tx_queue_tail = NULL;

uint8_t nrf_tx_enqueue(nrf_frame_t frame){
	nrf_tx_queue_node_t* new_node = (nrf_tx_queue_node_t*)malloc(sizeof(nrf_tx_queue_node_t));
	memcpy(new_node->frame.data, frame.data, 32);
	new_node->next = NULL;
	if(nrf_tx_queue_tail == NULL){
		nrf_tx_queue_tail = new_node;
		nrf_tx_queue_head = new_node;
	}else{
		nrf_tx_queue_tail->next = new_node;
		nrf_tx_queue_tail = new_node;
	}
	return 0;
}

uint8_t nrf_tx_dequeue(nrf_frame_t* frame){
	nrf_tx_queue_node_t* top_node;
	if(nrf_tx_queue_head == NULL){
		return 1;
	}
	top_node = nrf_tx_queue_head;
	if(top_node->next == NULL){
		nrf_tx_queue_tail = NULL;
		nrf_tx_queue_head = NULL;
	}else{
		nrf_tx_queue_head = top_node->next;
	}
	*frame = top_node->frame;
	free(top_node);
	return 0;
}

nrf_frame_t data_frame(uint8_t destination, uint8_t* data, uint8_t length){
	nrf_frame_t frame;
	memset(frame.data, 0, 32);
	if(length > 27 || length < 1){
		return frame;
	}
	frame.data[0] = __FRAME_DATA + length;
	frame.data[1]= destination;
	frame.data[2]= node_address;
	frame.data[3] = seq_id++;
	frame.data[4] = __TTL;
	memcpy(frame.data+5, data, length);
	return frame;
}

uint8_t retransmit_frame(nrf_frame_t frame){
	frame.data[4]--;
	if(frame.data[4] == 0){
		return 1;
	}
	nrf_tx_enqueue(frame);
	return 0;
}

uint8_t send_serial_frame(nrf_frame_t frame){
	uint8_t str[32];
	uint8_t length = frame.data[0] % 32;
	sprintf(str, "%03u", frame.data[2]);
	str[3] = ':';
	str[4] = ' ';
	memcpy(str+5, frame.data+5, length);
	str[length+5] = '\0';
	CDC_Transmit_FS(str, length+5);
	return 0;
}

uint8_t check_retransmission(nrf_frame_t frame){
	if(frame.data[2] == node_address){
		return 1;
	}
	if(frame.data[3] > members[frame.data[2]] || ((members[frame.data[2]] > 200) &&  (frame.data[3] < 50 ))){
		return 0;
	}
	return 1;
}

uint8_t encapsulate(uint8_t* data, uint16_t length){
	if(length <= 1){
		return 1;
	}
	uint8_t dest = data[0];
	if(dest == 0){
		return 1;
	}
	data++;
	length--;
	uint8_t size = 0;
	nrf_frame_t frame;
	while(1){
		if(length > 27){
			size = 27;
		}else{
			size = length;
		}
		frame = data_frame(dest, data, size);
		nrf_tx_enqueue(frame);
		if(length < 27){
			break;
		}
		length-=27;
		data+=27;
	}
	return 0;
}

uint8_t nrf_transmit_queue(){
	NRF24_TxMode(TxAddress, common_channel);
	nrf_frame_t frame;
	while(nrf_tx_dequeue(&frame) == 0){
		NRF24_Transmit(frame.data, 32);
	}
	NRF24_RxMode(RxAddress, common_channel);
	return 0;
}

uint8_t while_true_func(){
	if(node_address == 0){
		return 1; // Not configured
	}
	if (isDataAvailable(2) == 1){
		HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, SET);
		nrf_frame_t frame;
		NRF24_Receive(frame.data, 32);
		if(check_retransmission(frame) == 0){
			if(frame.data[1] == node_address){
				send_serial_frame(frame);
			}else{
				if(frame.data[1] == 255){
					send_serial_frame(frame);
				}
				retransmit_frame(frame);
			}
		}
		members[frame.data[2]] = frame.data[3]; // Set last received Seq_id
		HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, RESET);
	}else if(nrf_tx_queue_head != NULL){
		HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, SET);
		nrf_transmit_queue();
		HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, RESET);
	}
	return 0;
}
