#include "vitals_algo.h"
#include "config.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "VITALS";

typedef struct {
    int index;
    float value;
} peak_t;

/*----------------------------------------------------Blood Oxygen--------------------------------------------------------------------*/
float calculate_spo2(uint16_t red[], uint16_t ir[], int size)
{
    uint16_t max_val = 0;
    uint16_t min_val = 65535;
    float mean = 0.0f;
    if (size < 10) return -1;

    for (int i = 0; i < size; i++) {
        mean += red[i];
        if (red[i] > max_val) max_val = red[i];
        if (red[i] < min_val) min_val = red[i];
    }
    mean /= size;

    float threshold = mean;

    if (threshold < 3000) {
        printf("threshold = %f", threshold);
        printf("No finger Present ");
        return -1.0f;
    }

    float redDC = 0.0f, irDC = 0.0f;

    static float prev_spo2 = 100.0f;

    // Compute DC component
    for (int i = 0; i < size; i++) {
        redDC += red[i];
        irDC  += ir[i];
    }
    redDC /= size;
    irDC  /= size;

    // Compute AC (RMS)
    float redAC = 0.0f, irAC = 0.0f;

    for (int i = 0; i < size; i++) {
        float redDiff = (float)red[i] - redDC;
        float irDiff  = (float)ir[i]  - irDC;

        redAC += redDiff * redDiff;
        irAC  += irDiff  * irDiff;
    }

    redAC = sqrtf(redAC / size);
    irAC  = sqrtf(irAC / size);

    if (irAC <= 0.0f || redAC <= 0.0f) return -1;

    float ratio = (redAC / redDC) / (irAC / irDC);

    // Empirical linear formula
    float spo2 = 110.0f - 25.0f * ratio;

    if (spo2 > 100) spo2 = 100;
    if (spo2 < 50)  spo2 = 50;

    // Low-pass filtering
    const float alpha = 0.1f;
    spo2 = alpha * spo2 + (1.0f - alpha) * prev_spo2;

    prev_spo2 = spo2;

    return spo2;
}

/*----------------------------Heart Rate------------------------------------------*/

// Gaussian kernel for smoothing
static const float gaussian_kernel[VITALS_GAUSSIAN_KERNEL_SIZE] = {
    0.02763f, 0.06628f, 0.12383f, 0.18017f,
    0.20416f,
    0.18017f, 0.12383f, 0.06628f, 0.02763f
};

// Function to apply Gaussian smoothing
static void smooth_signal(const uint16_t *input, float *output, int size) {
    int half_kernel = VITALS_GAUSSIAN_KERNEL_SIZE / 2;

    for (int i = 0; i < size; i++) {
        output[i] = 0.0f;
        float weight_sum = 0.0f;

        for (int j = -half_kernel; j <= half_kernel; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < size) {
                output[i] += input[idx] * gaussian_kernel[j + half_kernel];
                weight_sum += gaussian_kernel[j + half_kernel];
            }
        }

        // Normalize by actual weight sum to handle boundaries
        if (weight_sum > 0.0f) {
            output[i] /= weight_sum;
        }
    }
}

// Function to detect peaks in the smoothed signal
static int detect_peaks(const float *signal, int size, peak_t *peaks, int max_peaks) {
    int half_window = VITALS_PEAK_WINDOW_SIZE / 2;
    int peak_count = 0;

    for (int i = half_window; i < size - half_window && peak_count < max_peaks; i++) {
        int is_peak = 1;
        float current_val = signal[i];

        // Check if current point is the maximum in the window
        for (int j = i - half_window; j <= i + half_window; j++) {
            if (j != i && signal[j] >= current_val) {
                is_peak = 0;
                break;
            }
        }

        if (is_peak) {
            peaks[peak_count].index = i;
            peaks[peak_count].value = current_val;
            peak_count++;
        }
    }

    return peak_count;
}

// Function to calculate heart rate from IR data
float calculate_heart_rate(uint16_t ir[], int size) {
    static float filtered_bpm = 75.0f;
    static float prev_filtered_bpm = 75.0f;
    // mean
    uint16_t max_val = 0;
    uint16_t min_val = 65535;
    float mean = 0.0f;

    for (int i = 0; i < size; i++) {
        mean += ir[i];
        if (ir[i] > max_val) max_val = ir[i];
        if (ir[i] < min_val) min_val = ir[i];
    }
    mean /= size;

    float range = max_val - min_val;
    if (range < 50) {
        printf("Signal too weak(No signal Detected): range=%.0f\n", range);
        return -1.0f;
    }

    float threshold = mean + range * 0.5f;

    if (threshold < 2000) {
        printf("threshold = %f", threshold);
        printf("No finger Present ");
        return -1.0f;
    }

    if (size < 100) {
        ESP_LOGE(TAG, "Insufficient data points: %d", size);
        return 0.0f;
    }

    // Allocate memory for smoothed signal
    float *smoothed_ir = (float *)malloc(size * sizeof(float));
    if (!smoothed_ir) {
        ESP_LOGE(TAG, "Failed to allocate memory for smoothed signal");
        return 0.0f;
    }

    // Apply Gaussian smoothing
    smooth_signal(ir, smoothed_ir, size);

    // Allocate memory for peaks (maximum possible peaks)
    int max_possible_peaks = size / (VITALS_PEAK_WINDOW_SIZE / 2);
    peak_t *peaks = (peak_t *)malloc(max_possible_peaks * sizeof(peak_t));
    if (!peaks) {
        ESP_LOGE(TAG, "Failed to allocate memory for peaks");
        free(smoothed_ir);
        return 0.0f;
    }

    // Detect peaks
    int peak_count = detect_peaks(smoothed_ir, size, peaks, max_possible_peaks);

    ESP_LOGI(TAG, "Detected %d peaks", peak_count);

    // Calculate heart rate if we have enough peaks
    if (peak_count > 1) {
        // Calculate RR intervals in samples
        float *rr_intervals_sec = (float *)malloc((peak_count - 1) * sizeof(float));
        if (!rr_intervals_sec) {
            ESP_LOGE(TAG, "Failed to allocate memory for RR intervals");
            free(smoothed_ir);
            free(peaks);
            return 0.0f;
        }

        // Convert peak differences to RR intervals in seconds
        for (int i = 0; i < peak_count - 1; i++) {
            int rr_interval_samples = peaks[i + 1].index - peaks[i].index;
            rr_intervals_sec[i] = rr_interval_samples / VITALS_SAMPLE_RATE;
        }

        // Filter RR intervals for plausible heart rates
        float min_rr = 60.0f / VITALS_MAX_BPM;  // Minimum 180 BPM
        float max_rr = 60.0f / VITALS_MIN_BPM;  // Maximum 40 BPM

        float sum_valid_rr = 0.0f;
        int valid_rr_count = 0;

        for (int i = 0; i < peak_count - 1; i++) {
            if (rr_intervals_sec[i] >= min_rr && rr_intervals_sec[i] <= max_rr) {
                sum_valid_rr += rr_intervals_sec[i];
                valid_rr_count++;
            }
        }

        ESP_LOGI(TAG, "Valid RR intervals: %d out of %d", valid_rr_count, peak_count - 1);

        if (valid_rr_count > 0) {
            float avg_rr = sum_valid_rr / valid_rr_count;
            filtered_bpm = 60.0f / avg_rr;

            ESP_LOGI(TAG, "Calculated BPM: %.2f", filtered_bpm);
        } else {
            ESP_LOGW(TAG, "No valid RR intervals found for possible heart rates");
            filtered_bpm = 0.0f;
        }

        free(rr_intervals_sec);
    } else {
        ESP_LOGW(TAG, "Not enough peaks to calculate average BPM");
        filtered_bpm = 0.0f;
    }

    // Clean
    free(smoothed_ir);
    free(peaks);
    // low pass filter to make smooth change of the bpm
    const float alpha = 0.1f;

    filtered_bpm = alpha * filtered_bpm + (1.0f - alpha) * prev_filtered_bpm;
    prev_filtered_bpm = filtered_bpm;

    return filtered_bpm;
}
