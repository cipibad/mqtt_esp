#include "nvs.h"

#include "app_nvs.h"

esp_err_t write_nvs_integer(const char * tag, int value)
{
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    printf("Error (%d) opening NVS handle!\n", err);
  } else {
    printf("Done\n");
    printf("Updating %s in NVS ... ", tag);
    err = nvs_set_i32(my_handle, tag, value);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    printf("Committing updates in NVS ... ");
    err = nvs_commit(my_handle);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

    // Close
    nvs_close(my_handle);
  }
  return err;
}

esp_err_t read_nvs_integer(const char * tag, int * value)
{
  printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
  switch (err) {
  case ESP_ERR_NVS_NOT_FOUND:
    printf("The storage is not initialized yet!\n");
    err = ESP_OK;
    break;
  case ESP_OK:
    printf("Done\n");
    printf("Reading %s from NVS ... ", tag);
    err = nvs_get_i32(my_handle, tag, value);
    switch (err) {
    case ESP_OK:
      printf("Done\n");
      printf("%s = %d\n", tag, *value);
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      printf("The value is not initialized yet!\n");
      err = ESP_OK;
      break;
    default :
      printf("Error (%d) reading!\n", err);
    }
    break;
  default:
    printf("Error (%d) opening NVS handle!\n", err);
    break;
  }

  nvs_close(my_handle);
  return err;
}


esp_err_t write_nvs_short(const char * tag, short value)
{
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    printf("Error (%d) opening NVS handle!\n", err);
  } else {
    printf("Done\n");
    printf("Updating %s in NVS ... ", tag);
    err = nvs_set_i16(my_handle, tag, value);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    printf("Committing updates in NVS ... ");
    err = nvs_commit(my_handle);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

    // Close
    nvs_close(my_handle);
  }
  return err;
}

esp_err_t read_nvs_short(const char * tag, short * value)
{
  printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
  switch (err) {
  case ESP_ERR_NVS_NOT_FOUND:
    printf("The storage is not initialized yet!\n");
    err = ESP_OK;
    break;
  case ESP_OK:
    printf("Done\n");
    printf("Reading %s from NVS ... ", tag);
    err = nvs_get_i16(my_handle, tag, value);
    switch (err) {
    case ESP_OK:
      printf("Done\n");
      printf("%s = %d\n", tag, *value);
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      printf("The value is not initialized yet!\n");
      err = ESP_OK;
      break;
    default :
      printf("Error (%d) reading!\n", err);
    }
    break;
  default:
    printf("Error (%d) opening NVS handle!\n", err);
    break;
  }

  nvs_close(my_handle);
  return err;
}


esp_err_t write_nvs_str(const char * tag, char * value)
{
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    printf("Error (%d) opening NVS handle!\n", err);
  } else {
    printf("Done\n");
    printf("Updating %s in NVS ... ", tag);
    err = nvs_set_str(my_handle, tag, value);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    printf("Committing updates in NVS ... ");
    err = nvs_commit(my_handle);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

    // Close
    nvs_close(my_handle);
  }
  return err;
}

esp_err_t read_nvs_str(const char * tag, char * value, size_t * length)
{
  printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
  switch (err) {
  case ESP_ERR_NVS_NOT_FOUND:
    printf("The storage is not initialized yet!\n");
    err = ESP_OK;
    break;
  case ESP_OK:
    printf("Done\n");
    printf("Reading %s from NVS ... ", tag);
    err = nvs_get_str(my_handle, tag, value, length);
    switch (err) {
    case ESP_OK:
      printf("Done\n");
      printf("%s = %s\n", tag, value);
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      printf("The value is not initialized yet!\n");
      err = ESP_OK;
      break;
    default :
      printf("Error (%d) reading!\n", err);
    }
    break;
  default:
    printf("Error (%d) opening NVS handle!\n", err);
    break;
  }

  nvs_close(my_handle);
  return err;
}


esp_err_t write_nvs_blob(const char * tag, void * value, size_t length)
{
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    printf("Error (%d) opening NVS handle!\n", err);
  } else {
    printf("Updating %s in NVS ... ", tag);
    err = nvs_set_blob(my_handle, tag, value, length);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    printf("Committing updates in NVS ... ");
    err = nvs_commit(my_handle);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

    nvs_close(my_handle);
  }
  return err;
}

esp_err_t read_nvs_blob(const char * tag, void * value, size_t * length)
{
  printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
  switch (err) {
  case ESP_ERR_NVS_NOT_FOUND:
    printf("The storage is not initialized yet!\n");
    err = ESP_OK;
    *length = 0;
    break;
  case ESP_OK:
    printf("Done\n");
    printf("Reading %s from NVS ... ", tag);
    err = nvs_get_blob(my_handle, tag, value, length);
    switch (err) {
    case ESP_OK:
      printf("Done\n");
      printf("%s = %u bytes\n", tag, (unsigned)*length);
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      printf("The value is not initialized yet!\n");
      *length = 0;
      err = ESP_OK;
      break;
    default :
      printf("Error (%d) reading!\n", err);
    }
    break;
  default:
    printf("Error (%d) opening NVS handle!\n", err);
    break;
  }

  nvs_close(my_handle);
  return err;
}
