#ifndef PPSP_EXPERIMENT_H
#define PPSP_EXPERIMENT_H

int experiment_run_all(const char *data_dir, const char *output_dir,
                       char *error, unsigned long error_size);
int experiment_run_all_parameters(const char *data_dir, const char *output_dir,
                                  double annual_discount_rate,
                                  double internal_ratio_penalty_rate,
                                  char *error, unsigned long error_size);

#endif
