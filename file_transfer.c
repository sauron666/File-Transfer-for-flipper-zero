#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <usb/usb.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>
#include <string.h>

#define HID_REPORT_SIZE 64

typedef enum {
    MenuOption_Send,
    MenuOption_Receive,
    MenuOption_USBSettings,
    MenuOption_StaticDynamicSwitch,
    MenuOption_Exit,
    MenuOption_Count
} MenuOption;

typedef enum {
    Mode_None,
    Mode_Done
} TransferMode;

TransferMode current_mode = Mode_None;
MenuOption selected_option = MenuOption_Send;

typedef enum {
    USBConfig_Static,
    USBConfig_Dynamic
} USBConfigMode;

USBConfigMode usb_config_mode = USBConfig_Static;

// USB properties
typedef struct {
    uint16_t vid;
    uint16_t pid;
    char serial[16];
    char manufacturer[32];
    char product[32];
    char revision[8];
} USBSettings;

USBSettings static_usb_settings = {
    .vid = 0x1234,
    .pid = 0x5678,
    .serial = "STATIC001",
    .manufacturer = "StaticDevices",
    .product = "StaticTransfer",
    .revision = "1.0",
};

USBSettings dynamic_usb_settings = {
    .vid = 0x8765,
    .pid = 0x4321,
    .serial = "DYNAMIC001",
    .manufacturer = "DynamicDevices",
    .product = "DynamicTransfer",
    .revision = "2.0",
};

// Apply USB settings
void apply_usb_settings() {
    USBSettings* current_settings =
        (usb_config_mode == USBConfig_Static) ? &static_usb_settings : &dynamic_usb_settings;

    USBDevice* usb = furi_record_open(RECORD_USB);
    usb_set_vid(usb, current_settings->vid);
    usb_set_pid(usb, current_settings->pid);
    usb_set_serial(usb, current_settings->serial);
    usb_set_manufacturer(usb, current_settings->manufacturer);
    usb_set_product(usb, current_settings->product);
    usb_set_revision(usb, current_settings->revision);
    furi_record_close(RECORD_USB);

    FURI_LOG_I("USBSettings", "USB settings applied: %s mode",
               (usb_config_mode == USBConfig_Static) ? "Static" : "Dynamic");
}

// Receive file
void receive_file() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_open(storage, "/ext/received_file.txt", FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if (!file) {
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E("FileTransfer", "Failed to create file for receiving data");
        return;
    }

    USBDevice* usb = furi_record_open(RECORD_USB);
    uint8_t buffer[HID_REPORT_SIZE];
    while (true) {
        usb_hid_read(usb, buffer, HID_REPORT_SIZE);
        if (buffer[0] == 0xFF) break;  // End-of-file marker
        storage_file_write(file, buffer, HID_REPORT_SIZE);
    }

    storage_file_close(file);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_USB);
    FURI_LOG_I("FileTransfer", "File successfully received and saved");
}

// Send file
void send_file() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_open(storage, "/ext/received_file.txt", FSAM_READ, FSOM_OPEN_EXISTING);

    if (!file) {
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E("FileTransfer", "File not found for sending");
        return;
    }

    USBDevice* usb = furi_record_open(RECORD_USB);
    uint8_t buffer[HID_REPORT_SIZE];
    ssize_t read_bytes;
    while ((read_bytes = storage_file_read(file, buffer, HID_REPORT_SIZE)) > 0) {
        usb_hid_write(usb, buffer, read_bytes);
    }

    // Send EOF marker
    memset(buffer, 0xFF, HID_REPORT_SIZE);
    usb_hid_write(usb, buffer, HID_REPORT_SIZE);

    storage_file_close(file);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_USB);
    FURI_LOG_I("FileTransfer", "File successfully sent");
}

// GUI: Draw main menu
void draw_main_menu(Canvas* canvas, void* ctx) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Draw menu title
    canvas_draw_str(canvas, 10, 20, "File Transfer");

    // Menu options
    const char* options[MenuOption_Count] = {
        "Send File", "Receive File", "USB Settings", "Switch USB Mode", "Exit"};

    for (int i = 0; i < MenuOption_Count; i++) {
        if (i == selected_option) {
            canvas_draw_str(canvas, 5, 40 + (i * 20), ">");
        }
        canvas_draw_str(canvas, 20, 40 + (i * 20), options[i]);
    }
}

// GUI: Draw USB settings
void draw_usb_settings(Canvas* canvas, void* ctx) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    USBSettings* current_settings =
        (usb_config_mode == USBConfig_Static) ? &static_usb_settings : &dynamic_usb_settings;

    canvas_draw_str(canvas, 10, 20, "Current USB Settings:");
    char buffer[64];

    snprintf(buffer, sizeof(buffer), "VID: %04X", current_settings->vid);
    canvas_draw_str(canvas, 10, 40, buffer);

    snprintf(buffer, sizeof(buffer), "PID: %04X", current_settings->pid);
    canvas_draw_str(canvas, 10, 60, buffer);

    snprintf(buffer, sizeof(buffer), "Serial: %s", current_settings->serial);
    canvas_draw_str(canvas, 10, 80, buffer);

    snprintf(buffer, sizeof(buffer), "Manuf: %s", current_settings->manufacturer);
    canvas_draw_str(canvas, 10, 100, buffer);

    snprintf(buffer, sizeof(buffer), "Product: %s", current_settings->product);
    canvas_draw_str(canvas, 10, 120, buffer);

    snprintf(buffer, sizeof(buffer), "Rev: %s", current_settings->revision);
    canvas_draw_str(canvas, 10, 140, buffer);
}

// Input callback
void input_callback(InputEvent* input_event, void* ctx) {
    if (input_event->type == InputTypePress) {
        switch (input_event->key) {
            case InputKeyUp:
                if (selected_option > 0) {
                    selected_option--;
                }
                break;

            case InputKeyDown:
                if (selected_option < MenuOption_Count - 1) {
                    selected_option++;
                }
                break;

            case InputKeyOk:
                if (selected_option == MenuOption_Send) {
                    send_file();
                } else if (selected_option == MenuOption_Receive) {
                    receive_file();
                } else if (selected_option == MenuOption_USBSettings) {
                    apply_usb_settings();
                } else if (selected_option == MenuOption_StaticDynamicSwitch) {
                    usb_config_mode = (usb_config_mode == USBConfig_Static) ? USBConfig_Dynamic
                                                                           : USBConfig_Static;
                    apply_usb_settings();
                } else if (selected_option == MenuOption_Exit) {
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

// Main app function
int32_t file_transfer_app(void* p) {
    Gui* gui = furi_record_open(RECORD_GUI);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, draw_main_menu, NULL);
    view_port_input_callback_set(viewport, input_callback, NULL);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    while (current_mode != Mode_Done) {
        view_port_update(viewport);
        furi_delay_ms(100);
    }

    gui_remove_view_port(gui, viewport);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    return 0;
}
