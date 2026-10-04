#include "system/wheel_encoder_state.h"

#include <assert.h>
#include <stdio.h>

static void expect_near(float actual, float expected)
{
  assert(fabsf(actual - expected) < 0.0001f);
}

int main()
{
  WheelEncoderState wheel;
  wheel.update(1.0f, 1000000000ULL);
  expect_near(wheel.position, 1.0f);
  expect_near(wheel.velocity, 0.0f);

  wheel.update(1.25f, 1250000000ULL);
  expect_near(wheel.velocity, 1.0f);
  wheel.update(2.0f, 1250000000ULL);
  expect_near(wheel.position, 1.25f);
  expect_near(wheel.velocity, 1.0f);

  wheel.update(1.1f, 1500000000ULL);
  expect_near(wheel.velocity, -0.6f);
  wheel.update(1.1f, 1600000000ULL);
  expect_near(wheel.velocity, 0.0f);

  wheel.update(0.4f, 100ULL);
  expect_near(wheel.position, 0.4f);
  expect_near(wheel.velocity, 0.0f);
  wheel.update(0.6f, 200000100ULL);
  expect_near(wheel.velocity, 1.0f);

  constexpr float radians_per_degree = 0.017453292519943295f;
  WheelEncoderState forward;
  forward.update(359.0f * radians_per_degree, 0);
  forward.update(1.0f * radians_per_degree, 100000000ULL);
  expect_near(forward.velocity, 20.0f * radians_per_degree);

  WheelEncoderState reverse;
  reverse.update(1.0f * radians_per_degree, 0);
  reverse.update(359.0f * radians_per_degree, 100000000ULL);
  expect_near(reverse.velocity, -20.0f * radians_per_degree);
  expect_near(forward.velocity, 20.0f * radians_per_degree);

  WheelEncoderState irregular;
  irregular.update(0.0f, 0);
  irregular.update(0.4f, 200000000ULL);
  expect_near(irregular.velocity, 2.0f);
  irregular.update(1.4f, 700000000ULL);
  expect_near(irregular.velocity, 2.0f);

  puts("PASS: encoder velocity timing, wraparound, reverse, stop, duplicate and reset");
  return 0;
}