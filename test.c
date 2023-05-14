
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>


typedef enum cfg_type{cfg_ptr, cfg_str, cfg_int, cfg_bool, cfg_hex, cfg_float, cfg_loglevel} cfg_type;

struct cfg_line {
  cfg_type type;
  void **value;
  void *value_ptr;
  char *description;
  //char name[20];
  char *name;
  int size;
};

struct test_cfg
{
  char *p_val;
  char c_val[30];
  bool b_val;
  //int b_val;
  int i_val;
  float f_val;
  unsigned char uc_val;
  unsigned int ul_val;
};


//struct cfg_line _cfg_lines[];
struct cfg_line *_cfg_lines;


void test_cfg_init (struct test_cfg *config_parameters);
void test_print_cfg();
void test_write_tocfg();
void test_free_cfg();
void set_cfg_param(struct cfg_line *cfg, char *name, void *ptr, cfg_type type, char *des);



void test_cfg_init (struct test_cfg *config_parameters) {
 
  config_parameters->p_val = "Initial p_val setting";
  strcpy(config_parameters->c_val, "Initial c_val setting");
  config_parameters->b_val = false;
  config_parameters->i_val = 10;
  config_parameters->f_val = 0.01;
  config_parameters->ul_val = 5;
  config_parameters->uc_val = 0x9a;

  set_cfg_param(&_cfg_lines[__COUNTER__], "pointer test", &config_parameters->p_val, cfg_ptr, "pointer test description");
  set_cfg_param(&_cfg_lines[__COUNTER__], "char test", &config_parameters->c_val, cfg_str,  "pointer char description");
  set_cfg_param(&_cfg_lines[__COUNTER__], "int test", &config_parameters->i_val, cfg_int, "int test description");
  set_cfg_param(&_cfg_lines[__COUNTER__], "float test", &config_parameters->f_val, cfg_float, "float test description");
  set_cfg_param(&_cfg_lines[__COUNTER__], "ulong test", &config_parameters->ul_val, cfg_loglevel, "ulong test description");
  set_cfg_param(&_cfg_lines[__COUNTER__], "hex test", &config_parameters->uc_val, cfg_hex, "hex test description");
  set_cfg_param(&_cfg_lines[__COUNTER__], "bool test", &config_parameters->b_val, cfg_bool, "bool test description");

  printf("Config init %30s = %s\n",_cfg_lines[0].name,(char *)*_cfg_lines[0].value);
  printf("Config init %30s = %s\n",_cfg_lines[1].name,(char *)_cfg_lines[1].value);
  printf("Config init %30s = %d\n",_cfg_lines[2].name,*((int *)_cfg_lines[2].value));
  printf("Config init %30s = %.3f\n",_cfg_lines[3].name,*((float *)_cfg_lines[3].value));
  printf("Config init %30s = %lu\n",_cfg_lines[4].name,*((unsigned long *)_cfg_lines[4].value));
  printf("Config init %30s = 0x%02hhx\n",_cfg_lines[5].name,*((unsigned char *)_cfg_lines[5].value));
  printf("Config init %30s = %d\n",_cfg_lines[6].name,*((bool *)_cfg_lines[6].value));
  //printf("Config init %30s = %d | %s\n",_cfg_lines[6].name,*((bool *)*_cfg_lines[6].value),*((bool *)*_cfg_lines[6].value)==true?"True":"False");
}

#define CFG_LEN __COUNTER__



#include "utils.h"

int main(int argc, char *argv[]) {

  int ppm = 2900;

  printf("Roundf %f\n",roundf(degFtoC(ppm)));

  printf("Convert %f\n",degFtoC(ppm));

  printf("Round %d\n",round(degFtoC(ppm)));


  return 0;


  struct test_cfg config_parameters;
/* 
  int num = 5;
  void *ptr = &num;
  void **xptr = &ptr;
  printf ("values:%d\n", *((int *)*xptr));  
*/
  _cfg_lines = (struct cfg_line*)malloc(sizeof(struct cfg_line) * CFG_LEN );

  bool val = false;
  void *pval;
  void **ppval;

  pval = &val;
  ppval = &pval;

  printf("%d %d %d\n",val,*((bool *)pval),*((bool *)*ppval));
  val = true;
  printf("%d %d %d\n",val,*((bool *)pval),*((bool *)*ppval));
  *((bool *)*ppval) = false;
  printf("%d %d %d\n",val,*((bool *)pval),*((bool *)*ppval));
   *((bool *)*ppval) = true;
  printf("%d %d %d\n",val,*((bool *)pval),*((bool *)*ppval));

  int ival = 11;
  void *pival;
  void **ppival;

  pival = &ival;
  ppival = &pival;

  printf("%d %d %d\n",ival,*((int *)pival),*((int *)*ppival));

  test_cfg_init(&config_parameters);
  
  printf("b_val = %d\n",config_parameters.b_val);
  //printf("b_val ptr = %d\n",_cfg_lines[6].value);
  //printf("b_val ptr = %d\n",&_cfg_lines[6].value);
  //printf("b_val ptr = %d\n",*_cfg_lines[6].value);

  //printf("b_val ptrs = %p %p %p\n",config_parameters.b_val,_cfg_lines[6].value_ptr,_cfg_lines[6].value);
  //return 0;
  //printf("c_val = %s\n",config_parameters.p_val);
  //printf("p_val = %s\n",config_parameters.p_val);
  //printf("c_val = %s\n",_cfg_lines[0].value);
  //printf("c_val = %s\n",&_cfg_lines[0].value);
  //printf("c_val = %s\n",*_cfg_lines[0].value);
  //config_parameters.b_val = true;
  //printf("c_val = %s\n",&(*_cfg_lines[0].value)[0]);

  test_print_cfg();

  config_parameters.b_val = true;
  strcpy(config_parameters.c_val, "Changed c_val setting");
  config_parameters.p_val = "Changed p_val setting";
  config_parameters.f_val = 1.02;
  config_parameters.i_val = 11;
  config_parameters.uc_val = 0x0b;
  config_parameters.ul_val = 6;
  printf("********* Changed by cfg**************\n");
  printf("b_val direct set to true= %d\n",config_parameters.b_val);
  test_print_cfg();

  test_write_tocfg();
  printf("b_val test_write_tocfg() = %d\n",config_parameters.b_val);
  printf("********* Changed by cfg address **************\n");
 // *_cfg_lines[0].value = "FINALLY";
  test_print_cfg();

  printf("********* actual **************\n");
  printf("p_val = %s\n",config_parameters.p_val);
  printf("c_val = %s\n",config_parameters.c_val);
  printf("i_val = %d\n",config_parameters.i_val);
  printf("f_val = %.2f\n",config_parameters.f_val);
  printf("ul_val = %u\n",config_parameters.ul_val);
  printf("uc_val = 0x%02hhx\n",config_parameters.uc_val);
  printf("bool_val = %d\n",config_parameters.b_val);

  test_free_cfg();

  return 0;
}

void test_free_cfg()
{
  int i;

  for (i=0; i < CFG_LEN; i++) {
    if (_cfg_lines[i].name == NULL) {
      continue;
    }
    if (_cfg_lines[i].type == cfg_ptr ) {
      printf("Free: %s\n",_cfg_lines[i].name);
      free(*_cfg_lines[i].value);
    }
  }
}
void test_write_tocfg()
{
  int i;

  for (i=0; i < CFG_LEN; i++) {
    if (_cfg_lines[i].name == NULL) {
      continue;
    }
    switch (_cfg_lines[i].type) { 
      case cfg_str:
        sprintf((char *)_cfg_lines[i].value, "Last value");
      break;
      case cfg_ptr:
      {
        char *val = malloc(sizeof(char *)*50);
        sprintf(val, "xxxxx");
        *_cfg_lines[i].value = val;
      }
      break;
      case cfg_int:
        *((int *)_cfg_lines[i].value) = 12;
      break;
      case cfg_hex:
        *((unsigned char *)_cfg_lines[i].value) = 0x78;
      break;
      case cfg_float:
        *((float *)_cfg_lines[i].value) = 999.99;
      break;
      case cfg_loglevel:
        *((unsigned long *)_cfg_lines[i].value) = 98;
      break;
      case cfg_bool:
       *((bool *)_cfg_lines[i].value) = false;
      break;
    }
  }
}

//void set_cfg_param(struct cfg_line *cfg, void *ptr, char *name, char *des, cfg_type type) {
void set_cfg_param(struct cfg_line *cfg, char *name, void *ptr, cfg_type type, char *des) {

  cfg->type = type;
  cfg->name = name;
  cfg->description = des;   

  switch (type) {
    
    case cfg_ptr:  
      cfg->value = ptr;
    break;

    case cfg_str:
    case cfg_bool:
    case cfg_int:
    case cfg_float:
    case cfg_loglevel:
    case cfg_hex:
      cfg->value_ptr = ptr;
      cfg->value = cfg->value_ptr;
    break;
  }
}

void test_print_cfg()
{
  int i;

  for (i=0; i < CFG_LEN; i++) {
    if (_cfg_lines[i].name == NULL) {
      continue;
    }
    switch (_cfg_lines[i].type) {
      case cfg_bool:
       printf("Config %30s = %d | %s\n",_cfg_lines[i].name,*((bool *)_cfg_lines[i].value),*((bool *)_cfg_lines[i].value)==true?"True":"False");
      
      break;
      case cfg_str:
        printf("Config %30s = %s\n",_cfg_lines[i].name,(char *)_cfg_lines[i].value);
      break;
      case cfg_ptr:
        printf("Config %30s = %s\n",_cfg_lines[i].name,(char *)*_cfg_lines[i].value);
      break;
      case cfg_int:
        printf("Config %30s = %d\n",_cfg_lines[i].name,*((int *)_cfg_lines[i].value));
      break;
      case cfg_hex:
        printf("Config %30s = 0x%02hhx\n",_cfg_lines[i].name,*((unsigned char *)_cfg_lines[i].value));
      break;
      case cfg_float:
        printf("Config %30s = %.2f\n",_cfg_lines[i].name,*((float *)_cfg_lines[i].value));
      break;
      case cfg_loglevel:
        printf("Config %30s = %lu\n",_cfg_lines[i].name,*((unsigned long *)_cfg_lines[i].value));
      break;
    }
  }
}