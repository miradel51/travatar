#include <boost/foreach.hpp>
#include <map>
#include <fstream>
#include <lm/model.hh>
#include <travatar/hyper-graph.h>
#include <travatar/eval-measure.h>
#include <travatar/dict.h>
#include <travatar/util.h>
#include <travatar/weights.h>
#include <travatar/lm-composer-bu.h>

using namespace std;
using namespace travatar;
using namespace boost;

#define NGRAM_ORDER 5
#define NBEST_COUNT 10
#define POP_LIMIT 500

// Find the oracle sentence for this evaluatio measure
Sentence EvalMeasure::CalculateOracle(const HyperGraph & graph, const Sentence & ref) {
    Sentence bord_ref;
    // Create the bordered sentence
    bord_ref.push_back(Dict::WID("<s>"));
    BOOST_FOREACH(WordId wid, ref) bord_ref.push_back(wid);
    bord_ref.push_back(Dict::WID("</s>"));
    // Create the n-grams
    typedef map<vector<WordId>, int> NgramMap;
    vector<NgramMap> ngrams(NGRAM_ORDER+1);
    int act_order = 0;
    for(int i = 0; i < (int)bord_ref.size(); i++) {
        vector<WordId> curr;
        for(int j = 0; j <= NGRAM_ORDER; j++) {
            ngrams[j][curr]++;
            act_order = max(j, act_order);
            if(i+j >= (int)bord_ref.size()) break;
            curr.push_back(bord_ref[i+j]);
        }
    }
    // Write the arpa file
    ofstream ofs("/tmp/oracle.arpa");
    if(!ofs) THROW_ERROR("Could not open /tmp/oracle.arpa for writing");
    ofs << "\\data\\\n";
    for(int n = 1; n <= act_order; n++) {
        int size = ngrams[n].size() + (n == 1 ? 1 : 0);
        ofs << "ngram " << n << "=" << size << endl;
    }
    for(int n = 1; n <= act_order; n++) {
        if(n != 1 && ngrams[n].size() == 0) break;
        ofs << endl << "\\"<<n<<"-grams:"<<endl;
        if(n == 1) ofs << "-99\t<unk>\t-99" << endl;
        BOOST_FOREACH(const NgramMap::value_type & val, ngrams[n]) {
            vector<WordId> context = val.first;
            context.resize(context.size()-1);
            ofs << log(val.second)-log(ngrams[n-1][context]) << "\t" << Dict::PrintWords(val.first);
            if(n != act_order) ofs << "\t-99";
            ofs << endl;
        }
    }
    ofs << "\\end\\" << endl << endl;
    // Load the LM
    lm::ngram::Config config;
    config.messages = NULL;
    LMComposerBU bu(new lm::ngram::Model("/tmp/oracle.arpa", config));
    bu.SetFeatureName("oraclelm");
    bu.SetLMWeight(1);
    bu.SetStackPopLimit(POP_LIMIT);
    // Decode with the reference
    HyperGraph rescored_graph(graph);
    Weights empty_weights;
    rescored_graph.ScoreEdges(empty_weights);
    shared_ptr<HyperGraph> lm_graph(bu.TransformGraph(rescored_graph));
    // Create n-best list
    NbestList nbest_list = lm_graph->GetNbest(NBEST_COUNT, rescored_graph.GetWords());
    // Find the sentence in the n-best list with the highest score
    Sentence ret; 
    double best_score = 0;
    BOOST_FOREACH(const shared_ptr<HyperPath> & path, nbest_list) {
        Sentence curr = path->GetWords();
        double score = this->CalculateStats(ref, curr)->ConvertToScore();
        if(score > best_score) {
            ret = curr;
            best_score = score;
        }
    }
    // Return the sentence
    return ret;
}
