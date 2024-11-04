#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

typedef enum {
    Mode_None,
    Mode_Send,
    Mode_Receive
} TransferMode;

TransferMode current_mode = Mode_None;

// Функция за записване на данни във файл
void receive_file() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_open(storage, "/ext/received_file.txt", FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if (!file) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    const char* data = "Example data to store in file";
    storage_file_write(file, data, strlen(data));
    storage_file_close(file);
    furi_record_close(RECORD_STORAGE);
}

// Функция за четене на данни от файл
void send_file() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_open(storage, "/ext/received_file.txt", FSAM_READ, FSOM_OPEN_EXISTING);

    if (file) {
        char buffer[64];
        storage_file_read(file, buffer, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';  // Добавяне на край на стринга
        // Може да се използва buffer за показване на данните на дисплея или други операции
        storage_file_close(file);
    }
    furi_record_close(RECORD_STORAGE);
}

// Графичен интерфейс на основното меню
void draw_main_menu(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 20, "File Transfer");
    canvas_draw_str(canvas, 10, 40, "1. Send File");
    canvas_draw_str(canvas, 10, 60, "2. Receive File");
    canvas_draw_str(canvas, 10, 80, "Press 1 or 2 to choose");
}

// Обработка на входните данни
void input_callback(InputEvent* input_event, void* ctx) {
    if(input_event->type == InputTypePress) {
        if(input_event->key == InputKey1) {
            current_mode = Mode_Send;
        } else if(input_event->key == InputKey2) {
            current_mode = Mode_Receive;
        }
    }
}

// Основна функция на приложението
int32_t file_transfer_app(void* p) {
    Gui* gui = furi_record_open(RECORD_GUI);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, draw_main_menu, NULL);
    view_port_input_callback_set(viewport, input_callback, NULL);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    while(current_mode == Mode_None) {
        furi_delay_ms(100);
    }

    if(current_mode == Mode_Send) {
        notification_message(notification, &sequence_set_only_red);
        send_file();
    } else if(current_mode == Mode_Receive) {
        notification_message(notification, &sequence_set_only_green);
        receive_file();
    }

    notification_message(notification, &sequence_reset_red);
    gui_remove_view_port(gui, viewport);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    return 0;
}
