#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>
#include <usb/class/hid/usb_hid.h>
#include <string.h>

typedef enum {
    Mode_None,
    Mode_Send,
    Mode_Receive
} TransferMode;

// Глобална променлива за текущия режим на трансфер
TransferMode current_mode = Mode_None;

// Функция за инициализация на HID интерфейса
void setup_hid() {
    usb_init();
    usb_hid_init();
    usb_start();
}

// Функция за изпращане на файл към компютъра през HID
void send_file(const char* file_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_open(storage, file_path, FSAM_READ, FSOM_OPEN_EXISTING);

    if(file) {
        uint8_t buffer[1];
        while(storage_file_read(file, buffer, sizeof(buffer)) > 0) {
            usb_hid_send(buffer, sizeof(buffer));
        }
        // Изпраща специален байт за край на трансфера
        buffer[0] = 0xFF;
        usb_hid_send(buffer, sizeof(buffer));
        storage_file_close(file);
    }
    furi_record_close(RECORD_STORAGE);
}

// Функция за получаване на файл от компютъра през HID
void receive_file(const char* save_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_open(storage, save_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if(file) {
        uint8_t buffer[1];
        while(true) {
            usb_hid_recv(buffer, sizeof(buffer));
            if(buffer[0] == 0xFF) break;  // Край на трансфера
            storage_file_write(file, buffer, sizeof(buffer));
        }
        storage_file_close(file);
    }
    furi_record_close(RECORD_STORAGE);
}

// Основна функция за графичния интерфейс на приложението
void draw_main_menu(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 20, 20, "File Transfer");
    canvas_draw_str(canvas, 10, 40, "1. Send File");
    canvas_draw_str(canvas, 10, 60, "2. Receive File");
    canvas_draw_str(canvas, 10, 80, "Press 1 or 2 to choose");
}

// Обработка на входа от бутоните
void input_callback(InputEvent* input_event, void* ctx) {
    if(input_event->type == InputTypePress) {
        if(input_event->key == InputKey1) {
            current_mode = Mode_Send;
        } else if(input_event->key == InputKey2) {
            current_mode = Mode_Receive;
        }
    }
}

// Основна функция за приложението
int32_t file_transfer_app(void* p) {
    Gui* gui = furi_record_open(RECORD_GUI);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    // Инициализация на HID
    setup_hid();

    // Настройка на графичния интерфейс и обработка на входа
    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, draw_main_menu, NULL);
    view_port_input_callback_set(viewport, input_callback, NULL);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    while(current_mode == Mode_None) {
        furi_delay_ms(100);
    }

    if(current_mode == Mode_Send) {
        notification_message(notification, &sequence_set_only_red);
        const char* file_path = "/ext/your_file.bin";  // Път до файла на SD картата
        send_file(file_path);
    } else if(current_mode == Mode_Receive) {
        notification_message(notification, &sequence_set_only_green);
        const char* save_path = "/ext/received_file.bin";
        receive_file(save_path);
    }

    notification_message(notification, &sequence_reset_red);
    gui_remove_view_port(gui, viewport);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    return 0;
}
