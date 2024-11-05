#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>
#include <usb/usb.h>
#include <usb/hid/hid.h>

typedef enum {
    MenuOption_Send,
    MenuOption_Receive,
    MenuOption_Exit,
    MenuOption_Count  // Брой опции в менюто
} MenuOption;

typedef enum {
    Mode_None,
    Mode_Done
} TransferMode;

TransferMode current_mode = Mode_None;
MenuOption selected_option = MenuOption_Send;

// Функция за записване на данни във файл
void receive_file() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        FURI_LOG_E("FileTransfer", "Failed to open storage");
        return;
    }
    File* file = storage_file_open(storage, "/ext/received_file.bin", FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if (!file) {
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E("FileTransfer", "Failed to create file for receiving data");
        return;
    }

    uint8_t buffer[64];
    size_t received_bytes = 0;
    while ((received_bytes = usb_hid_receive(buffer, sizeof(buffer))) > 0) {
        storage_file_write(file, buffer, received_bytes);
    }

    storage_file_close(file);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I("FileTransfer", "File successfully received and saved");
}

// Функция за четене на данни от файл и изпращането им през HID
void send_file() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if (!storage) {
        FURI_LOG_E("FileTransfer", "Failed to open storage");
        return;
    }
    File* file = storage_file_open(storage, "/ext/received_file.bin", FSAM_READ, FSOM_OPEN_EXISTING);

    if (file) {
        uint8_t buffer[64];
        ssize_t read_bytes = 0;
        while ((read_bytes = storage_file_read(file, buffer, sizeof(buffer))) > 0) {
            usb_hid_send(buffer, read_bytes);
        }
        storage_file_close(file);
    } else {
        FURI_LOG_E("FileTransfer", "File not found for sending");
    }

    furi_record_close(RECORD_STORAGE);
}

// Графичен интерфейс на основното меню
void draw_main_menu(Canvas* canvas, void* ctx) {
    (void)ctx;  // Unused parameter
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Рисуване на менюто
    canvas_draw_str(canvas, 10, 20, "File Transfer");

    // Опции в менюто
    const char* options[MenuOption_Count] = {"Send File", "Receive File", "Exit"};

    for(int i = 0; i < MenuOption_Count; i++) {
        if(i == selected_option) {
            // Показване на селектора (стрелка) за избраната опция
            canvas_draw_str(canvas, 5, 40 + (i * 20), ">");
        }
        canvas_draw_str(canvas, 20, 40 + (i * 20), options[i]);
    }
}

// Обработка на входните данни
void input_callback(InputEvent* input_event, void* ctx) {
    (void)ctx;  // Unused parameter
    if(input_event->type == InputTypePress) {
        switch(input_event->key) {
            case InputKeyUp:
                if(selected_option > 0) {
                    selected_option--;
                }
                break;

            case InputKeyDown:
                if(selected_option < MenuOption_Count - 1) {
                    selected_option++;
                }
                break;

            case InputKeyOk:
                if(selected_option == MenuOption_Send) {
                    send_file();
                } else if(selected_option == MenuOption_Receive) {
                    receive_file();
                } else if(selected_option == MenuOption_Exit) {
                    current_mode = Mode_Done;
                }
                break;

            case InputKeyBack:
                current_mode = Mode_Done;
                break;

            default:
                break;
        }
    }
}

// Основна функция на приложението
int32_t file_transfer_app(void* p) {
    (void)p;  // Unused parameter
    Gui* gui = furi_record_open(RECORD_GUI);
    if (!gui) {
        FURI_LOG_E("FileTransfer", "Failed to open GUI");
        return -1;
    }

    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, draw_main_menu, NULL);
    view_port_input_callback_set(viewport, input_callback, NULL);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    while(current_mode != Mode_Done) {
        view_port_update(viewport);  // Актуализиране на изгледа
        furi_delay_ms(100);
    }

    // Почистване и излизане от приложението
    gui_remove_view_port(gui, viewport);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);

    return 0;
}
