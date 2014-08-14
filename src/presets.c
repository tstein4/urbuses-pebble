#include <pebble.h>
#include "presets.h"
#include "libs/linked-list/linked-list.h"
#include "libs/message-queue/message-queue.h"
#include "libs/data-processor/data-processor.h"
#include "preset.h"

#define STORAGE_PRESET_START 2
#define PRESET_BLOCK_SIZE 5

static void eta_handler(char* operation, char* data);
static void process_eta_data(char* data);
static void process_preset_data(char* data);
static void tick_callback(struct tm *tick_time, TimeUnits units_changed);

typedef struct PresetBlock {
	Preset presets[PRESET_BLOCK_SIZE];
	uint8_t count;
} PresetBlock;

static LinkedRoot* presets = NULL;

void presets_init(void){
	mqueue_register_handler("PRESET_ETA", eta_handler);
	presets = linked_list_create_root();
	tick_timer_service_subscribe(MINUTE_UNIT, tick_callback);
}

void presets_deinit(){
	tick_timer_service_unsubscribe();
	presets_save();
	presets_clear();
	free(presets);
}

Preset* presets_get(int pos){
	return (Preset*) linked_list_get(presets, pos);
}

int presets_get_count(){
	return linked_list_count(presets);
}

void presets_add(Preset* preset){
	linked_list_append(presets, preset);
}

void presets_clear(){
	while (presets_get_count() > 0){
		presets_remove(0);
	}
}

void presets_remove(int pos){
	Preset* preset = presets_get(pos);
	free(preset);
	linked_list_remove(presets, pos);
}

void presets_restore(void){
	presets_clear();

	if (!persist_exists(STORAGE_PRESET_START)){
		return;
	}

	int block = 0;
	PresetBlock* presetBlock = malloc(sizeof(PresetBlock));
	persist_read_data(STORAGE_PRESET_START, presetBlock, sizeof(PresetBlock));

	uint8_t preset_count = presetBlock->count;

	for (int i = 0; i < preset_count; i++){
		if (i > 0 && i % PRESET_BLOCK_SIZE == 0){
			block += 1;
			free(presetBlock);
			presetBlock = malloc(sizeof(PresetBlock));
			persist_read_data(STORAGE_PRESET_START + block, presetBlock, sizeof(PresetBlock));
		}
		Preset *preset = preset_clone(&presetBlock->presets[i % PRESET_BLOCK_SIZE]);
		presets_add(preset);
	}
	free(presetBlock);
	return;
}

void presets_save(void){
	int block = 0;
	uint8_t num_presets = presets_get_count();
	if (num_presets == 0){
		persist_delete(STORAGE_PRESET_START);
		return;
	}
	for (int i = 0; i < num_presets; i+= PRESET_BLOCK_SIZE){
		PresetBlock *presetBlock = malloc(sizeof(PresetBlock));
		presetBlock->count = num_presets;
		for (int j = 0; j < PRESET_BLOCK_SIZE; j++){
			if (i+j >= num_presets)
				break;
			presetBlock->presets[j] = *presets_get(i+j);
		}
		persist_write_data(STORAGE_PRESET_START+block, presetBlock, sizeof(PresetBlock));
		free(presetBlock);
		block++;
	}
	return;
}


void decrement_etas(){
	for (int i = 0; i < presets_get_count(); i++){
		Preset *temp = presets_get(i);
		
		temp->eta -= 1;
		if (temp->eta < 0){
			// send_eta_req(temp);
		}
	}
}

static void tick_callback(struct tm *tick_time, TimeUnits units_changed)
{
	if (tick_time->tm_min % 10 == 0){
		// send_all_eta_req();
	}
	else
	{
		decrement_etas();
	}
}


static void eta_handler(char* operation, char* data){
	if (strcmp(operation, "PRESET_ETA") == 0){
		process_eta_data(data);
	}
	else if (strcmp(operation, "PRESET_SET") == 0){
		process_preset_data(data);
	}
}

static void process_eta_data(char* data){
	data_processor_init(data, '|');
	char* stop_id = data_processor_get_string();
	char* route_id = data_processor_get_string();
	int eta = atoi(data_processor_get_string());
	for (int i = 0; i < presets_get_count(); i++){
		Preset *preset = presets_get(i);
		if (strcmp(preset->stop_id, stop_id) == 0 && strcmp(preset->route_id, route_id) == 0){
			preset->eta = eta;
			break;
		}
	}
	free(stop_id);
	free(route_id);
}

static void process_preset_data(char* data){
	data_processor_init(data, '|');
	Preset *preset = malloc(sizeof(Preset));
	strcpy(preset->stop_id, data_processor_get_string());
	strcpy(preset->stop_name, data_processor_get_string());
	strcpy(preset->route_id, data_processor_get_string());
	strcpy(preset->route_name, data_processor_get_string());
	presets_add(preset);
}