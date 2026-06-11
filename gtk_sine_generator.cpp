#include <gtk/gtk.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <math.h>
#include <pthread.h>
#include <atomic>
#include <cstring>

// Audio parameters
struct AudioParams {
    float amplitude;
    float frequency;
    float phase;
    bool muted;
    std::atomic<bool> running;
    pthread_t audio_thread;
    
    AudioParams() : amplitude(0.5f), frequency(500.0f), phase(0.0f), muted(false), running(true) {}
};

AudioParams params;

// Audio thread function
void* audio_thread_func(void* arg) {
    AudioParams* p = (AudioParams*)arg;
    
    // PulseAudio configuration
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_FLOAT32,
        .rate = 44100,
        .channels = 2
    };
    
    pa_simple* s = nullptr;
    int error;
    
    while (p->running) {
        if (!p->muted) {
            if (s == nullptr) {
                s = pa_simple_new(nullptr, "Sine Generator", PA_STREAM_PLAYBACK,
                                nullptr, "Sine Wave", &ss, nullptr, nullptr, &error);
                if (!s) {
                    fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
                    break;
                }
            }
            
            // Generate audio buffer
            const int buffer_size = 441; // 10ms at 44.1kHz
            float buffer[buffer_size * 2]; // stereo
            
            static double time = 0.0;
            const double dt = 1.0 / 44100.0;
            
            for (int i = 0; i < buffer_size; i++) {
                float sample = p->amplitude * sin(2.0 * M_PI * p->frequency * time + p->phase);
                buffer[i * 2] = sample;     // left channel
                buffer[i * 2 + 1] = sample; // right channel
                time += dt;
                
                if (time >= 1e6) time = 0.0; // prevent overflow
            }
            
            if (pa_simple_write(s, buffer, sizeof(buffer), &error) < 0) {
                fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(error));
                break;
            }
        } else {
            if (s != nullptr) {
                pa_simple_drain(s, &error);
                pa_simple_free(s);
                s = nullptr;
            }
            usleep(10000); // 10ms sleep when muted
        }
    }
    
    if (s != nullptr) {
        pa_simple_drain(s, &error);
        pa_simple_free(s);
    }
    
    return nullptr;
}

// GUI callbacks
void on_volume_changed(GtkRange* range, gpointer data) {
    double value = gtk_range_get_value(range);
    params.amplitude = pow(10.0, value / 20.0);
}

void on_phase_changed(GtkRange* range, gpointer data) {
    params.phase = gtk_range_get_value(range);
}

void on_frequency_changed(GtkRange* range, gpointer data) {
    params.frequency = gtk_range_get_value(range);
}

void on_mute_toggled(GtkToggleButton* button, gpointer data) {
    params.muted = gtk_toggle_button_get_active(button);
}

void on_destroy(GtkWidget* widget, gpointer data) {
    params.running = false;
    pthread_join(params.audio_thread, nullptr);
    gtk_main_quit();
}

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);
    
    // Create main window
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Simple Sine Wave Generator");
    gtk_window_set_default_size(GTK_WINDOW(window), 480, 140);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), nullptr);
    
    // Create vertical box
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Volume slider
    GtkWidget* volume_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), volume_box, FALSE, FALSE, 0);
    
    GtkWidget* volume_label = gtk_label_new("Volume:");
    gtk_box_pack_start(GTK_BOX(volume_box), volume_label, FALSE, FALSE, 0);
    
    GtkWidget* volume_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -96, 6, 0.1);
    gtk_range_set_value(GTK_RANGE(volume_slider), -6);
    gtk_widget_set_hexpand(volume_slider, TRUE);
    g_signal_connect(volume_slider, "value-changed", G_CALLBACK(on_volume_changed), nullptr);
    gtk_box_pack_start(GTK_BOX(volume_box), volume_slider, TRUE, TRUE, 0);
    
    // Phase slider
    GtkWidget* phase_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), phase_box, FALSE, FALSE, 0);
    
    GtkWidget* phase_label = gtk_label_new("Phase:");
    gtk_box_pack_start(GTK_BOX(phase_box), phase_label, FALSE, FALSE, 0);
    
    GtkWidget* phase_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.01);
    gtk_range_set_value(GTK_RANGE(phase_slider), 0);
    gtk_widget_set_hexpand(phase_slider, TRUE);
    g_signal_connect(phase_slider, "value-changed", G_CALLBACK(on_phase_changed), nullptr);
    gtk_box_pack_start(GTK_BOX(phase_box), phase_slider, TRUE, TRUE, 0);
    
    // Frequency slider
    GtkWidget* freq_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), freq_box, FALSE, FALSE, 0);
    
    GtkWidget* freq_label = gtk_label_new("Freq:");
    gtk_box_pack_start(GTK_BOX(freq_box), freq_label, FALSE, FALSE, 0);
    
    GtkWidget* freq_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 10, 22000, 1);
    gtk_range_set_value(GTK_RANGE(freq_slider), 500);
    gtk_widget_set_hexpand(freq_slider, TRUE);
    g_signal_connect(freq_slider, "value-changed", G_CALLBACK(on_frequency_changed), nullptr);
    gtk_box_pack_start(GTK_BOX(freq_box), freq_slider, TRUE, TRUE, 0);
    
    // Mute button
    GtkWidget* mute_button = gtk_toggle_button_new_with_label("Mute");
    g_signal_connect(mute_button, "toggled", G_CALLBACK(on_mute_toggled), nullptr);
    gtk_box_pack_start(GTK_BOX(vbox), mute_button, FALSE, FALSE, 0);
    
    // Start audio thread
    pthread_create(&params.audio_thread, nullptr, audio_thread_func, &params);
    
    // Show all widgets
    gtk_widget_show_all(window);
    
    // Run GTK main loop
    gtk_main();
    
    return 0;
}
