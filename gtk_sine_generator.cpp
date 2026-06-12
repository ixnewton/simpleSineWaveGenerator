#include <gtk/gtk.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <math.h>
#include <pthread.h>
#include <atomic>
#include <cstring>
#include <fstream>

// Audio parameters
struct AudioParams {
    float amplitude;
    float frequency;
    float phase;
    bool muted;
    std::atomic<bool> running;
    pthread_t audio_thread;
    
    // Sweep buffer
    float* sweep_buffer;
    int sweep_buffer_size;
    int sweep_buffer_position;
    bool sweep_playing;
    bool sweep_just_completed;
    
    // GUI widget reference for updating frequency slider
    GtkWidget* freq_slider;
    GtkWidget* play_mute_button;
    
    AudioParams() : amplitude(0.1f), frequency(500.0f), phase(0.0f), muted(true), running(true),
                    sweep_buffer(nullptr), sweep_buffer_size(0), sweep_buffer_position(0), 
                    sweep_playing(false), sweep_just_completed(false), freq_slider(nullptr), play_mute_button(nullptr) {}
};

AudioParams params;

// WAV file writer for debug
void write_wav_file(const char* filename, const float* buffer, int size, int sample_rate, int channels) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        printf("Failed to open WAV file: %s\n", filename);
        return;
    }
    
    int data_size = size * sizeof(float);
    int total_size = 36 + data_size;
    
    // RIFF header
    file.write("RIFF", 4);
    file.write((char*)&total_size, 4);
    file.write("WAVE", 4);
    
    // fmt chunk
    file.write("fmt ", 4);
    int fmt_size = 16;
    file.write((char*)&fmt_size, 4);
    short audio_format = 3; // IEEE float
    file.write((char*)&audio_format, 2);
    file.write((char*)&channels, 2);
    file.write((char*)&sample_rate, 4);
    int byte_rate = sample_rate * channels * sizeof(float);
    file.write((char*)&byte_rate, 4);
    short block_align = channels * sizeof(float);
    file.write((char*)&block_align, 2);
    short bits_per_sample = 32;
    file.write((char*)&bits_per_sample, 2);
    
    // data chunk
    file.write("data", 4);
    file.write((char*)&data_size, 4);
    file.write((char*)buffer, data_size);
    
    file.close();
    printf("WAV file written: %s\n", filename);
}

// Function to generate logarithmic sweep buffer
void generate_sweep_buffer(float start_freq, float end_freq, float duration, bool sweep_up) {
    // Free existing buffer if any
    if (params.sweep_buffer != nullptr) {
        delete[] params.sweep_buffer;
        params.sweep_buffer = nullptr;
    }
    
    // Calculate buffer size (44.1kHz * duration * 2 channels)
    int sample_rate = 44100;
    int total_samples = (int)(sample_rate * duration);
    params.sweep_buffer_size = total_samples * 2; // stereo
    params.sweep_buffer = new float[params.sweep_buffer_size];
    
    // Initialize buffer to zero
    memset(params.sweep_buffer, 0, params.sweep_buffer_size * sizeof(float));
    
    // Generate logarithmic sweep
    double dt = 1.0 / sample_rate;
    double time = 0.0;
    double phase = 0.0;
    
    for (int i = 0; i < total_samples; i++) {
        double current_freq;
        double progress = (double)i / (total_samples - 1); // Ensure last sample is exactly at end
        
        // Linear sweep in log space (logarithmic frequency sweep)
        // f(t) = f_start * (f_end/f_start)^(t/duration)
        double ratio = end_freq / start_freq;
        current_freq = start_freq * pow(ratio, progress);
        
        // Generate sine sample with phase accumulation for smooth frequency transitions
        float sample = params.amplitude * sin(phase);
        phase += 2.0 * M_PI * current_freq * dt;
        
        // Store in stereo buffer
        params.sweep_buffer[i * 2] = sample;     // left
        params.sweep_buffer[i * 2 + 1] = sample; // right
        
        time += dt;
    }
    
    params.sweep_buffer_position = 0;
    
    // Write to WAV file for debug
    const char* filename = sweep_up ? "sweep_up.wav" : "sweep_down.wav";
    write_wav_file(filename, params.sweep_buffer, params.sweep_buffer_size, sample_rate, 2);
}

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
        // Keep audio connection open if sweep is playing or if not muted
        if (!p->muted || p->sweep_playing) {
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
            
            if (p->sweep_playing && p->sweep_buffer != nullptr) {
                // Play from pre-generated sweep buffer (independent of mute state)
                int samples_to_copy = buffer_size;
                
                // Check if we have enough samples left in sweep buffer
                // samples_to_copy is in frames, need to multiply by 2 for stereo samples
                if (p->sweep_buffer_position + samples_to_copy * 2 > p->sweep_buffer_size) {
                    samples_to_copy = (p->sweep_buffer_size - p->sweep_buffer_position) / 2;
                }
                
                // Copy samples from sweep buffer
                for (int i = 0; i < samples_to_copy; i++) {
                    buffer[i * 2] = p->sweep_buffer[p->sweep_buffer_position + i * 2];
                    buffer[i * 2 + 1] = p->sweep_buffer[p->sweep_buffer_position + i * 2 + 1];
                }
                
                p->sweep_buffer_position += samples_to_copy * 2;
                
                // If sweep buffer is finished, stop playing and mute
                if (p->sweep_buffer_position >= p->sweep_buffer_size) {
                    p->sweep_playing = false;
                    p->sweep_buffer_position = 0;
                    p->sweep_just_completed = true;
                    p->muted = true;
                    
                    // Update GUI button to show "Play"
                    if (p->play_mute_button != nullptr) {
                        g_idle_add([](gpointer data) -> gboolean {
                            AudioParams* p = (AudioParams*)data;
                            gtk_button_set_label(GTK_BUTTON(p->play_mute_button), "Play");
                            return G_SOURCE_REMOVE;
                        }, p);
                    }
                }
                
                // Fill remaining buffer with silence if needed
                for (int i = samples_to_copy; i < buffer_size; i++) {
                    buffer[i * 2] = 0.0f;
                    buffer[i * 2 + 1] = 0.0f;
                }
            } else if (!p->muted) {
                // Generate continuous sine wave at current frequency (only when not muted)
                static double time = 0.0;
                const double dt = 1.0 / 44100.0;
                
                for (int i = 0; i < buffer_size; i++) {
                    float sample = p->amplitude * sin(2.0 * M_PI * p->frequency * time + p->phase);
                    buffer[i * 2] = sample;     // left channel
                    buffer[i * 2 + 1] = sample; // right channel
                    time += dt;
                    
                    if (time >= 1e6) time = 0.0; // prevent overflow
                }
            } else {
                // Output silence when muted and not playing sweep
                for (int i = 0; i < buffer_size; i++) {
                    buffer[i * 2] = 0.0f;
                    buffer[i * 2 + 1] = 0.0f;
                }
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

void on_play_mute_clicked(GtkButton* button, gpointer data) {
    params.muted = !params.muted;
    
    // Update button text based on new state
    if (params.muted) {
        gtk_button_set_label(button, "Play");
    } else {
        gtk_button_set_label(button, "Mute");
    }
}

void on_sweep_up_clicked(GtkButton* button, gpointer data) {
    GtkWidget** widgets = (GtkWidget**)data;
    
    // Get sweep parameters from GUI
    float start_freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets[0]));
    float end_freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets[1]));
    float duration = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets[2]));
    
    // Generate sweep up buffer
    generate_sweep_buffer(start_freq, end_freq, duration, true);
    
    // Start playing sweep
    params.sweep_playing = true;
    params.sweep_buffer_position = 0;
}

void on_sweep_down_clicked(GtkButton* button, gpointer data) {
    GtkWidget** widgets = (GtkWidget**)data;
    
    // Get sweep parameters from GUI
    float start_freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets[0]));
    float end_freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets[1]));
    float duration = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets[2]));
    
    // Sweep down plays from End -> Start (reverse order)
    generate_sweep_buffer(end_freq, start_freq, duration, false);
    
    // Start playing sweep
    params.sweep_playing = true;
    params.sweep_buffer_position = 0;
}

void on_destroy(GtkWidget* widget, gpointer data) {
    params.running = false;
    pthread_join(params.audio_thread, nullptr);
    
    // Clean up sweep buffer
    if (params.sweep_buffer != nullptr) {
        delete[] params.sweep_buffer;
        params.sweep_buffer = nullptr;
    }
    
    gtk_main_quit();
}

int main(int argc, char* argv[]) {
    // Ensure sweep buffer is cleared on launch
    if (params.sweep_buffer != nullptr) {
        delete[] params.sweep_buffer;
        params.sweep_buffer = nullptr;
    }
    params.sweep_buffer_size = 0;
    params.sweep_buffer_position = 0;
    params.sweep_playing = false;
    params.sweep_just_completed = false;
    
    gtk_init(&argc, &argv);
    
    // Create main window
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Simple Sine Wave Generator");
    gtk_window_set_default_size(GTK_WINDOW(window), 480, 250);
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
    gtk_range_set_value(GTK_RANGE(volume_slider), -20);
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
    
    // Store frequency slider reference for sweep updates
    params.freq_slider = freq_slider;
    
    // Play/Mute button
    GtkWidget* play_mute_button = gtk_button_new_with_label("Play");
    g_signal_connect(play_mute_button, "clicked", G_CALLBACK(on_play_mute_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(vbox), play_mute_button, FALSE, FALSE, 0);
    
    // Store button reference
    params.play_mute_button = play_mute_button;
    
    // Separator
    GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
    
    // Sweep controls
    GtkWidget* sweep_label = gtk_label_new("Frequency Sweep:");
    gtk_box_pack_start(GTK_BOX(vbox), sweep_label, FALSE, FALSE, 0);
    
    // Start frequency
    GtkWidget* start_freq_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), start_freq_box, FALSE, FALSE, 0);
    
    GtkWidget* start_freq_label = gtk_label_new("Start (Hz):");
    gtk_box_pack_start(GTK_BOX(start_freq_box), start_freq_label, FALSE, FALSE, 0);
    
    GtkWidget* start_freq_spin = gtk_spin_button_new_with_range(10, 22000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(start_freq_spin), 100);
    gtk_widget_set_hexpand(start_freq_spin, TRUE);
    gtk_box_pack_start(GTK_BOX(start_freq_box), start_freq_spin, TRUE, TRUE, 0);
    
    // End frequency
    GtkWidget* end_freq_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), end_freq_box, FALSE, FALSE, 0);
    
    GtkWidget* end_freq_label = gtk_label_new("End (Hz):");
    gtk_box_pack_start(GTK_BOX(end_freq_box), end_freq_label, FALSE, FALSE, 0);
    
    GtkWidget* end_freq_spin = gtk_spin_button_new_with_range(10, 22000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(end_freq_spin), 1000);
    gtk_widget_set_hexpand(end_freq_spin, TRUE);
    gtk_box_pack_start(GTK_BOX(end_freq_box), end_freq_spin, TRUE, TRUE, 0);
    
    // Duration
    GtkWidget* duration_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), duration_box, FALSE, FALSE, 0);
    
    GtkWidget* duration_label = gtk_label_new("Time (s):");
    gtk_box_pack_start(GTK_BOX(duration_box), duration_label, FALSE, FALSE, 0);
    
    GtkWidget* duration_spin = gtk_spin_button_new_with_range(0.1, 60, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(duration_spin), 5.0);
    gtk_widget_set_hexpand(duration_spin, TRUE);
    gtk_box_pack_start(GTK_BOX(duration_box), duration_spin, TRUE, TRUE, 0);
    
    // Sweep buttons container
    GtkWidget* sweep_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), sweep_button_box, FALSE, FALSE, 0);
    
    // Sweep down button
    GtkWidget* sweep_down_button = gtk_button_new_with_label("Sweep <-");
    
    // Create array of widgets for callback
    GtkWidget* sweep_widgets[3] = {start_freq_spin, end_freq_spin, duration_spin};
    g_signal_connect(sweep_down_button, "clicked", G_CALLBACK(on_sweep_down_clicked), sweep_widgets);
    gtk_box_pack_start(GTK_BOX(sweep_button_box), sweep_down_button, TRUE, TRUE, 0);
    
    // Sweep up button
    GtkWidget* sweep_up_button = gtk_button_new_with_label("Sweep ->");
    g_signal_connect(sweep_up_button, "clicked", G_CALLBACK(on_sweep_up_clicked), sweep_widgets);
    gtk_box_pack_start(GTK_BOX(sweep_button_box), sweep_up_button, TRUE, TRUE, 0);
    
    // Start audio thread
    pthread_create(&params.audio_thread, nullptr, audio_thread_func, &params);
    
    // Show all widgets
    gtk_widget_show_all(window);
    
    // Run GTK main loop
    gtk_main();
    
    return 0;
}
