#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>

int CROSS_VALIDATION = 10;
int *true_positive, *true_negative, *false_positive, *false_negative;
int start_index, end_index, cross_threshold, total_test_examples, total_learn_examples;

char *directory;

//from http://www.cse.yorku.ca/~oz/hash.html
unsigned long hash(unsigned char *str){
  unsigned long hash = 5381;
  int c;

  while (c = *str++){
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

int compare_hashes(const void *a, const void *b){
  if(*(unsigned long*)a < *(unsigned long*)b){
    return -1;
  }

  if(*(unsigned long*)a > *(unsigned long*)b){
    return 1;
  }

  if(*(unsigned long*)a == *(unsigned long*)b){
    return 0;
  }
}

int binary_search(unsigned long key, unsigned long *array, int low, int high){
  int mid = (high + low) / 2;

  if (array[mid] == key){
    int total = 0, i;

    for(i = mid; i >= 0 && array[i] == key; i--){
      total++;
    }

    for(i = mid; array[i] == key; i++){
      total++;
    }

    return total - 1;
  }

  if(low < high){
    if(array[mid] > key){
      return binary_search(key, array, low, mid);
    }
    else{ 
      return binary_search(key, array, mid + 1, high);
    }
  }

  return 0;
}

void is_directory(){
  struct stat statbuf;

  if(stat(directory, &statbuf) == -1) {
    errx(-1, "stat on directory error, errno = %d, %s", errno, strerror(errno));
  }

  if(!S_ISDIR(statbuf.st_mode)){
    errx(-1, "First parameter must be a directory");
  }
}

int read_file(unsigned long **file, int index, FILE *fp){
  char buffer[200] = {'\0'}, c;
  int i = 0, j = 0;

  while ((c = fgetc(fp)) != EOF){
    if(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))){
      buffer[j++] = c;
    }

    if(j > 0 && (c == ' ' || c == '\n' || c == '.')){
      //add a new word, the next word
      if((file[index] = realloc(file[index], (i + 1) * sizeof(unsigned long))) == NULL) {
        errx(-1, "second level malloc error, errno = %d, %s", errno, strerror(errno));
      }

      //printf("Adding word '%s' at %d\n", buffer, i);

      file[index][i] = hash(buffer);
      memset(buffer, '\0', 200);

      j = 0;
      i++;
    }
  }

  return i;
}

int get_vocabulary(unsigned long **learn_examples, int *words_per_learn_example, unsigned long **vocabulary, int total_learn_examples){
  int i, j, k, found;
  int vocabulary_counter = 0;
  unsigned long *vocabulary_temp;

  //realloc later
  if((vocabulary_temp = malloc(sizeof(unsigned long))) == NULL) {
    errx(-1, "error on vocabulary malloc, errno = %d, %s", errno, strerror(errno));
  }

  //O(n^2) I know :-)
  for(i = 0; i < total_learn_examples; i++){
    for(j = 0; j < words_per_learn_example[i]; j++){
      found = 0;

      for(k = 0; k < vocabulary_counter; k++){
        if(vocabulary_temp[k] == learn_examples[i][j]){
          found = 1;
          break;
        }
      }

      if(!found){
        if((vocabulary_temp = realloc(vocabulary_temp, (vocabulary_counter + 1) * sizeof(unsigned long))) == NULL) {
          errx(-1, "second level malloc error, errno = %d, %s", errno, strerror(errno));
        }

        vocabulary_temp[vocabulary_counter] = learn_examples[i][j];
        vocabulary_counter++;
      }
    }
  }

  *vocabulary = vocabulary_temp;
  return vocabulary_counter;
}

void read_examples(unsigned long **examples, int *words_per_learn_example, int cross_learning_start_index, int start_index, int end_index, int total_examples){
  char file_path[strlen(directory) + 11]; // will append "/24999.txt" + 1 for \0
  int i, half;
  FILE *fp;
  half = ceil(total_examples / 2);

  for(i = 0; i < total_examples; i++){
    int file_index = ((i >= half ? i % half : i) + cross_learning_start_index) % (end_index + 1);
    snprintf(file_path, strlen(directory) + 15, "%s/%s/%d.txt", directory, i >= half ? "pos" : "neg", file_index);

    //printf("%s\n", file_path);

    if((fp = fopen(file_path, "r")) == NULL) {
      errx(-1, "File opening error, errno = %d, %s - %s", errno, strerror(errno), file_path);
    }

    if((examples[i] = malloc(sizeof(unsigned long))) == NULL) {
      errx(-1, "Second level malloc error, errno = %d, %s", errno, strerror(errno));
    }

    words_per_learn_example[i] = read_file(examples, i, fp);

    //printf("%d %d\n", i, words_per_learn_example[i]);

    fclose(fp);
  }
}

void *thread(void *arg){
  int *words_per_learn_example, *words_per_test_example;
  unsigned long *vocabulary;

  //first level: multiple files
  //second level: multiple words
  unsigned long **learn_examples;
  unsigned long **test_examples;

  uint i = (intptr_t)arg;
  int j, k, l;

  int cross_learning_start_index = cross_threshold * i + start_index;
  int cross_learning_end_index = cross_threshold * ((CROSS_VALIDATION - 1 + i) % (CROSS_VALIDATION)) - 1 + start_index;
  int cross_testing_start_index = cross_learning_end_index + 1;
  int cross_testing_end_index = cross_testing_start_index + cross_threshold - 1;

  if(cross_learning_end_index == -1){
    cross_learning_end_index = end_index;
  }

  if((words_per_learn_example = calloc(total_learn_examples, sizeof(int))) == NULL) {
    errx(-1, "error on words_per_learn_example malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((words_per_test_example = calloc(total_learn_examples, sizeof(int))) == NULL) {
    errx(-1, "error on words_per_test_example malloc, errno = %d, %s", errno, strerror(errno));
  }

  //first level malloc for learn_examples
  if((learn_examples = malloc(total_learn_examples * sizeof(unsigned long *))) == NULL) {
    errx(-1, "error on first level learn_examples malloc, errno = %d, %s", errno, strerror(errno));
  }

  //first level malloc for test_examples
  if((test_examples = malloc(total_test_examples * sizeof(unsigned long *))) == NULL) {
    errx(-1, "error on first level test_examples malloc, errno = %d, %s", errno, strerror(errno));
  }

  //1. Collect all words and other tokens that occur in Examples:
  read_examples(learn_examples, words_per_learn_example, cross_learning_start_index, start_index, end_index, total_learn_examples);

  //Vocabulary ← all distinct words and other tokens in Examples
  int vocabulary_length = get_vocabulary(learn_examples, words_per_learn_example, &vocabulary, total_learn_examples);

  read_examples(test_examples, words_per_test_example, cross_testing_start_index, start_index, end_index, total_test_examples);

  int N_pos = 0;
  int N_neg = 0;

  int learn_half_index = ceil(total_learn_examples / 2);
  int test_half_index = ceil(total_test_examples / 2);

  //n ← total number of words in Texti (counting duplicate words multiple times)
  //get total positive and negative words
  for(j = 0; j < total_learn_examples; j++){
    int index = (i >= learn_half_index ? i % learn_half_index : i);

    if(j >= learn_half_index){
      N_pos += words_per_learn_example[j];
    }
    else{
      N_neg += words_per_learn_example[j];
    }

    qsort(learn_examples[j], words_per_learn_example[j], sizeof(unsigned long), compare_hashes);
  }

  double *P_conditional_pos;
  double *P_conditional_neg;

  if((P_conditional_pos = calloc(vocabulary_length, sizeof(double))) == NULL) {
    errx(-1, "error on P_conditional_pos malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((P_conditional_neg = calloc(vocabulary_length, sizeof(double))) == NULL) {
    errx(-1, "error on P_conditional_neg malloc, errno = %d, %s", errno, strerror(errno));
  }

  //for each word Wj in Vocabulary
  for(j = 0; j < vocabulary_length; j++){
    //nj ← number of times word Wj occurs in Texti
    int n_pos = 0;
    int n_neg = 0;
    int total;

    for(k = 0; k < total_learn_examples; k++){
      total = binary_search(vocabulary[j], learn_examples[k], 0, words_per_learn_example[k] - 1);

      if(total){
        if(k >= learn_half_index){
          n_pos = total + n_pos;
        }
        else{
          n_neg = total + n_neg;
        }
      }
    }

    //P(Wj|Bi) ← (nj + 1) / (n + |Vocabulary|) using logarithm
    P_conditional_pos[j] = log((n_pos + 1) / (double) (N_pos + vocabulary_length));
    P_conditional_neg[j] = log((n_neg + 1) / (double) (N_neg + vocabulary_length));
  }

  //CLASSIFY_NAIVE_BAYES_TEXT(Doc)
  for(j = 0; j < total_test_examples; j++){
    double NB_pos = 0;
    double NB_neg = 0;

    for(k = 0; k < words_per_test_example[j]; k++){
      for(l = 0; l < vocabulary_length; l++){
        //all word positions in Doc that contain tokens found in Vocabulary
        //can't use binary search here because we need the indexes (we can but it would be too much work)
        if(vocabulary[l] == test_examples[j][k]){
          NB_pos += P_conditional_pos[l];
          NB_neg += P_conditional_neg[l];
        }
      }
    }

    if(NB_pos > NB_neg){
      if(j >= test_half_index){
        true_positive[i]++;
      }
      else{
        false_positive[i]++;
      }
    }
    else{
      if(j >= test_half_index){
        false_negative[i]++;
      }
      else{
        true_negative[i]++;
      }
    }
  }
}

int main(int argc, char **argv){
  int i;

  if(argc != 4){
    errx(-1, "Expecting 3 parameters: <IMDB directory> <start_index> <end_index>");
  }

  start_index = strtoull(argv[2], NULL, 10);
  end_index = strtoull(argv[3], NULL, 10);

  if((directory = malloc((1 + strlen(argv[1])) * sizeof(char))) == NULL) {
    errx(-1, "error on directory malloc, errno = %d, %s", errno, strerror(errno));
  }

  memset(directory, '\0', strlen(argv[1]) + 1); //make the string null terminated
  strncpy(directory, argv[1], strlen(argv[1]));

  is_directory(directory);
  
  if(end_index - start_index <= 0){
    errx(-1, "End index has to be larger than start index");
  }

  if((1 + end_index - start_index) % CROSS_VALIDATION != 0){
    errx(-1, "Total number of files (got %d) must be multiple of %d\n", 1 + end_index - start_index, CROSS_VALIDATION);
  }

  cross_threshold = ((1 + end_index - start_index) / CROSS_VALIDATION);
  total_test_examples = cross_threshold * 2; //number of files on each cross validation subset
  total_learn_examples = ((1 + end_index - start_index) * 2) - total_test_examples; //*2 because we have neg and pos subfolders

  printf("Reading from: %s\n", directory);
  printf("Start index: %d\n", start_index);
  printf("End index: %d\n", end_index);
  printf("Total examples to learn: %d\n", total_learn_examples);
  printf("Total examples to test: %d\n\n", total_test_examples);

  int true_positive_total = 0;
  int true_negative_total = 0;
  int false_positive_total = 0;
  int false_negative_total = 0;

  if((true_positive = calloc(CROSS_VALIDATION, sizeof(int))) == NULL) {
    errx(-1, "error on true_positive malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((true_negative = calloc(CROSS_VALIDATION, sizeof(int))) == NULL) {
    errx(-1, "error on true_negative malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((false_positive = calloc(CROSS_VALIDATION, sizeof(int))) == NULL) {
    errx(-1, "error on false_positive malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((false_negative = calloc(CROSS_VALIDATION, sizeof(int))) == NULL) {
    errx(-1, "error on false_negative malloc, errno = %d, %s", errno, strerror(errno));
  }

  //10 threads, I know
  pthread_t threads[CROSS_VALIDATION];

  for(i = 0; i < CROSS_VALIDATION; i++){
		if (pthread_create(&(threads[i]), NULL, &thread, (void *)(intptr_t)i) != 0){
			errx(-1, "can't create thread :[%s]");
		}
	}

	for(i = 0; i < CROSS_VALIDATION; i++){
		if(pthread_join(threads[i], NULL) != 0){
			errx(-1, "pthread_join %s");
		}
	}

  for(i = 0; i < CROSS_VALIDATION; i++){
    true_positive_total += true_positive[i];
    true_negative_total += true_negative[i];
    false_positive_total += false_positive[i];
    false_negative_total += false_negative[i];
  }

  float true_positive_mean = true_positive_total / (float) CROSS_VALIDATION;
  float true_negative_mean = true_negative_total / (float) CROSS_VALIDATION;
  float false_positive_mean = false_positive_total / (float) CROSS_VALIDATION;
  float false_negative_mean = false_negative_total / (float) CROSS_VALIDATION;

  float true_positive_sd = 0.0;
  float true_negative_sd = 0.0;
  float false_positive_sd = 0.0;
  float false_negative_sd = 0.0;

  for(i = 0; i < CROSS_VALIDATION; i++){
    true_positive_sd += pow(true_positive[i] - true_positive_mean, 2);
    true_negative_sd += pow(true_negative[i] - true_negative_mean, 2);
    false_positive_sd += pow(false_positive[i] - false_positive_mean, 2);
    false_negative_sd += pow(false_negative[i] - false_negative_mean, 2);
  }

  printf("Mean True Positive: %.3f (total %d), Standard Deviation: %.3f\n", true_positive_mean, true_positive_total, sqrt(true_positive_sd/(float)(CROSS_VALIDATION - 1)));
  printf("Mean True Negative: %.3f (total %d), Standard Deviation: %.3f\n", true_negative_mean, true_negative_total, sqrt(true_negative_sd/(float)(CROSS_VALIDATION - 1)));
  printf("Mean False Positive: %.3f (total %d), Standard Deviation: %.3f\n", false_positive_mean, false_positive_total, sqrt(false_positive_sd/(float)(CROSS_VALIDATION - 1)));
  printf("Mean False Negative: %.3f (total %d), Standard Deviation: %.3f\n\n", false_negative_mean, false_negative_total, sqrt(false_negative_sd/(float)(CROSS_VALIDATION - 1)));

  float recall = true_positive_total / (float) (true_positive_total + false_negative_total);
  float precision = true_positive_total / (float) (true_positive_total + false_positive_total);

  printf("True Positive Rate: %.3f\n", recall);
  printf("False Positive Rate: %.3f\n", false_positive_total / (float) (false_positive_total + true_negative_total));
  printf("F-Measure: %.3f\n", (2 * recall * precision) / (recall + precision));
}
