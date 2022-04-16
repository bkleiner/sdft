#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdft.h"

#define M_PI_F 3.14159265358979323846f

#define LOOPTIME 500
#define LOOPTIME_S (LOOPTIME * 1e-6f)

uint32_t min_uint32(uint32_t a, uint32_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

float constrainf(const float in, const float min, const float max) {
  if (in > max)
    return max;
  if (in < min)
    return min;
  return in;
}

#define sinPolyCoef3 -1.666665710e-1f // Double: -1.666665709650470145824129400050267289858e-1
#define sinPolyCoef5 8.333017292e-3f  // Double:  8.333017291562218127986291618761571373087e-3
#define sinPolyCoef7 -1.980661520e-4f // Double: -1.980661520135080504411629636078917643846e-4
#define sinPolyCoef9 2.600054768e-6f  // Double:  2.600054767890361277123254766503271638682e-6

float fastsin(float x) {
  const int32_t xint = x;

  if (xint < -32 || xint > 32)
    return 0.0f; // Stop here on error input (5 * 360 Deg)

  while (x > M_PI_F)
    x -= (2.0f * M_PI_F); // always wrap input angle to -PI..PI

  while (x < -M_PI_F)
    x += (2.0f * M_PI_F);

  if (x > (0.5f * M_PI_F))
    x = (0.5f * M_PI_F) - (x - (0.5f * M_PI_F)); // We just pick -90..+90 Degree

  else if (x < -(0.5f * M_PI_F))
    x = -(0.5f * M_PI_F) - ((0.5f * M_PI_F) + x);

  const float x2 = x * x;
  return x + x * x2 * (sinPolyCoef3 + x2 * (sinPolyCoef5 + x2 * (sinPolyCoef7 + x2 * sinPolyCoef9)));
}

float fastcos(float x) {
  return fastsin(x + (0.5f * M_PI_F));
}

void lpf(float *out, float in, float coeff) {
  *out = (*out) * coeff + in * (1 - coeff);
}

#include "sdft.c"

static sdft_t sdft;

const char *get_field(char *line, int num) {
  const char *tok;
  for (tok = strtok(line, ",");
       tok && *tok;
       tok = strtok(NULL, ",\n")) {
    if (!--num)
      return tok;
  }
  return NULL;
}

float get_field_float(const char *line, int num) {
  char *tmp = strdup(line);
  const char *str_val = get_field(tmp, num);
  const float val = strtof(str_val, NULL);
  free(tmp);
  return val;
}

int get_field_int(const char *line, int num) {
  char *tmp = strdup(line);
  const char *str_val = get_field(tmp, num);
  const int val = atoi(str_val);
  free(tmp);
  return val;
}

static const char *cmd = "gnuplot -e \" "
                         "set terminal png size 800,600; "
                         "set output 'output/%05d_%05d.png'; "
                         "set title 'Loop %05d'; "
                         "set xtics 0,50,500; "
                         "plot '-' title 'dft' with lines, '-' title 'raw' with impulse, '-' title 'filt' with impulse, '-' title 'bb' with impulse"
                         "\"";

void update(int loop, float sample, float peaks[]) {
  static uint32_t counter = 0;

  if (!sdft_push(&sdft, sample)) {
    return;
  }

  while (!sdft_update(&sdft))
    ;

  counter++;

  char cmd_buffer[512];

  sprintf(cmd_buffer, cmd, counter, loop, loop);
  printf("processing %05d %05d\n", counter, loop);

  FILE *pipe = popen(cmd_buffer, "w");

  float max = 0.0f;

  for (uint32_t i = 0; i < SDFT_BIN_COUNT; i++) {
    const float f_hz = i * SDFT_HZ_RESOLUTION;
    fprintf(pipe, "%f %f\n", f_hz, sdft.magnitude[i]);
    if (sdft.magnitude[i] > max) {
      max = sdft.magnitude[i];
    }
  }
  fprintf(pipe, "e\n");

  for (uint32_t p = 0; p < SDFT_PEAKS; p++) {
    const float f_hz = sdft.peak_indicies[p] * SDFT_HZ_RESOLUTION;
    fprintf(pipe, "%f %f\n", f_hz, sdft.peak_values[p]);
  }
  fprintf(pipe, "e\n");

  for (uint32_t p = 0; p < SDFT_PEAKS; p++) {
    fprintf(pipe, "%f %f\n", sdft.notch_hz[p], max);
  }
  fprintf(pipe, "e\n");

  for (uint32_t p = 0; p < SDFT_PEAKS; p++) {
    fprintf(pipe, "%f %f\n", peaks[p], max);
  }
  fprintf(pipe, "e\n");

  pclose(pipe);
}

int main(int argc, char const *argv[]) {
  sdft_init(&sdft);

#if 0
  char line[1024];
  FILE *stream = fopen("log3.csv", "r");
  while (fgets(line, 1024, stream)) {
    if (line[0] == '"') {
      continue;
    }

    int loop = get_field_int(line, 1);
    float sample = get_field_float(line, 26) / 1000.f;

    float peaks[SDFT_PEAKS];
    for (uint32_t p = 0; p < SDFT_PEAKS; p++) {
      peaks[p] = get_field_float(line, 37 + p);
    }

    update(loop, sample, peaks);
  }
#else
  const float phase_increment = 2.0f * M_PI_F * (1.0f / SDFT_SAMPLE_HZ);

  for (uint32_t i = 0; i < 100000; i++) {
    float sample = sinf(400.f * (float)(i)*phase_increment) + 2.0f * sinf(220.f * (float)(i)*phase_increment);
    float peaks[SDFT_PEAKS] = {0, 0, 0};

    update(i, sample, peaks);
  }
#endif

  return 0;
}
