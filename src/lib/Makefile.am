include $(top_srcdir)/common.am

TRAVATARCPP = \
	hyper-graph.cc \
	alignment.cc \
	binarizer.cc \
	binarizer-cky.cc \
	binarizer-directional.cc \
	caser.cc \
	config-base.cc \
	dict.cc \
	eval-measure.cc \
	eval-measure-loader.cc \
	eval-measure-bleu.cc \
	eval-measure-interp.cc \
	eval-measure-ribes.cc \
	eval-measure-nist.cc \
	eval-measure-ter.cc \
	eval-measure-wer.cc \
	eval-measure-zeroone.cc \
	gradient.cc \
	gradient-xeval.cc \
	input-file-stream.cc \
	io-util.cc \
	lm-composer.cc \
	lm-composer-bu.cc \
	lm-composer-incremental.cc \
	lm-func.cc \
	lookup-table.cc \
	lookup-table-cfglm.cc \
	lookup-table-fsm.cc \
	lookup-table-hash.cc \
	lookup-table-marisa.cc \
	mert-geometry.cc \
	rule-composer.cc \
	rule-fsm.cc \
	forest-extractor.cc \
	hiero-extractor.cc \
	rule-filter.cc \
	sparse-map.cc \
	thread-pool.cc \
	timer.cc \
	tokenizer.cc \
	tokenizer-penn.cc \
	tokenizer-runner.cc \
	translation-rule.cc \
	tree-io.cc \
	trimmer.cc \
	trimmer-nbest.cc \
	tune.cc \
	tune-greedy-mert.cc \
	tune-mert.cc \
	tune-online.cc \
	tune-lbfgs.cc \
	tuning-example-forest.cc \
	tuning-example-nbest.cc \
	unary-flattener.cc \
	util.cc \
	weights.cc \
	weights-pairwise.cc \
	weights-perceptron.cc \
	weights-average-perceptron.cc \
	weights-adagrad.cc \
	weights-online-pro.cc \
	word-splitter.cc \
	cfg-data.cc \
	word-splitter-compound.cc \
	word-splitter-regex.cc \
	batch-tune-runner.cc \
	travatar-runner.cc \
	forest-extractor-runner.cc \
	mt-evaluator-runner.cc \
	mt-segmenter-runner.cc \
	train-caser-runner.cc \
	tree-converter-runner.cc \
	rescorer-runner.cc \
	hiero-extractor-runner.cc \
	translation-rule-hiero.cc \
	eval-measure-adv-interp.cc \
	math-query.cc \
	output-collector.cc


AM_CXXFLAGS += -I$(srcdir)/../include -I$(srcdir)/../kenlm -I$(srcdir)/.. $(BOOST_CPPFLAGS)

lib_LTLIBRARIES = libtravatar.la

libtravatar_la_SOURCES = ${TRAVATARCPP}
libtravatar_la_LDFLAGS = -version-info 0:0:0
