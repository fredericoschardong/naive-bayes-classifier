A simple implementation of [Tom Mitchell's](http://personal.disco.unimib.it/Vanneschi/McGrawHill_-_Machine_Learning_-Tom_Mitchell.pdf) Learn_Naïve_Bayes_Text and Classify_Naïve_Bayes_Text routines in C.

In this example the documents are a dump of 25000 positive and 25000 negative movie feedbacks from IMDB split into two folders (IMDB/pos and IMDB/neg). There are two possible groups a document can be classified into: positive or negative.

To compile simply run:
`gcc -o main main.c -lm`

And then:
`./main IMDB 0 99`

Where the first parameter is the IMDB folder, second is the starting index and third is the last document index.

Here is what the output looks like:
```
Reading from: IMDB
Start index: 0
End index: 99
Total examples to learn: 180
Total examples to test: 20

Mean True Positive: 6.700, Standard Deviation: 2.111
Mean True Negative: 9.300, Standard Deviation: 0.823
Mean False Positive: 0.700, Standard Deviation: 0.823
Mean False Negative: 3.300, Standard Deviation: 2.111

True Positive Rate: 0.670
False Positive Rate: 0.070
F-Measure: 0.770
```

As you can see 90% of the documents from the range chosen are used as the training set and 10% is used for testing, that is not part of Mitchell's routine. Also, ten-fold cross validation is implemented so the Mean values are the result of the ten different learning and testing combinations as well as the TP and FP rates and F-Measure.
