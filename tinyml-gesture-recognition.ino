/*
Edge Impulse Project: https://studio.edgeimpulse.com/public/841552/live
*/

#include <gesture_recognition_inferencing.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft;

// Hardware pins
int lightSensor = WIO_LIGHT;
int redLED = D0;
int greenLED = D1;
int buzzerPin = WIO_BUZZER;

// Sampling settings
#define FREQUENCY_HZ 100
#define INTERVAL_MS (1000 / FREQUENCY_HZ)

// Buffer for sensor readings
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
static int featureIndex = 0;

// Timing
unsigned long lastSampleTime = 0;

// Confidence threshold
#define CONFIDENCE_THRESHOLD 0.6

void playBeep() {
    analogWrite(buzzerPin, 128);   // Turn on buzzer
    delay(500);                    // Sound for 500ms
    analogWrite(buzzerPin, 0);     // Turn off buzzer
}

void lightRedLED(bool on) {
    digitalWrite(redLED, on ? HIGH : LOW);
}

void lightGreenLED(bool on) {
    digitalWrite(greenLED, on ? HIGH : LOW);
}

void runInference() {
    ei_impulse_result_t result = { 0 };
    
    // Create signal from features
    signal_t featuresSignal;
    featuresSignal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    featuresSignal.get_data = &raw_feature_get_data;
    
    // Run classifier
    EI_IMPULSE_ERROR res = run_classifier(&featuresSignal, &result, false);
    
    if (res != EI_IMPULSE_OK) {
        Serial.print("ERR: Failed to run classifier (");
        Serial.print(res);
        Serial.println(")");
        return;
    }
    
    // Find best prediction
    float maxConfidence = 0;
    String predictedGesture = "none";
    
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.println("--- Predictions ---");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        String label = ei_classifier_inferencing_categories[i];
        float confidence = result.classification[i].value;
        
        tft.print(label);
        tft.print(": ");
        tft.println(confidence, 3);
        
        if (confidence > maxConfidence) {
            maxConfidence = confidence;
            predictedGesture = label;
        }
    }
    
    // Control outputs based on gesture
    if (maxConfidence >= CONFIDENCE_THRESHOLD) {
        tft.print("\nDetected: ");
        tft.print(predictedGesture);
        tft.print(" (");
        tft.print(maxConfidence * 100, 1);
        tft.println("%)");
        
        // Turn off everything first
        lightRedLED(false);
        lightGreenLED(false);
        
        // Activate appropriate output
        if (predictedGesture == "scissors") {
            lightRedLED(true);
            tft.println("   -> RED LED ON");
        }
        else if (predictedGesture == "rock") {
            lightGreenLED(true);
            tft.println("   -> GREEN LED ON");
        }
        else if (predictedGesture == "paper") {
            playBeep();
            tft.println("   -> BUZZER ON");
        }
    } else {
        // Low confidence
        lightRedLED(false);
        lightGreenLED(false);
        tft.println("\n>> Low confidence, no action");
    }
    
    tft.print("Timing: DSP ");
    tft.print(result.timing.dsp);
    tft.println(" ms");
    tft.print("Inference ");
    tft.print(result.timing.classification);
    tft.println(" ms\n");
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    
    Serial.println("Edge Impulse - Gesture Recognition");
    Serial.println("====================================");
    
    // Hardware setup
    pinMode(lightSensor, INPUT);
    pinMode(redLED, OUTPUT);
    pinMode(greenLED, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    
    // Turn off outputs
    lightRedLED(false);
    lightGreenLED(false);
    
    Serial.print("Model expects ");
    Serial.print(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    Serial.println(" features");
    Serial.println("Collecting samples...\n");

    tft.begin();
    tft.setRotation(3); // Set rotation
    tft.fillScreen(TFT_BLACK); // Clear screen with black
    tft.setTextSize(2.5);
}

void loop() {
    unsigned long currentTime = millis();
    
    // Sample at correct frequency (100 Hz)
    if (currentTime - lastSampleTime >= INTERVAL_MS) {
        lastSampleTime = currentTime;
        
        // Read light sensor
        int lightValue = analogRead(lightSensor);
        
        // Store in buffer
        features[featureIndex] = (float)lightValue;
        featureIndex++;
        
        // When buffer is full, run inference
        if (featureIndex >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            runInference();
            featureIndex = 0;  // Reset for next window
            delay(2000);  // Wait 2 seconds before next measurement
        }
    }
}

/**
 * @brief      Copy raw feature data in out_ptr
 *             Function called by inference library
 */
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}