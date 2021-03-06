#include <travatar/translation-rule-hiero.h>
#include <travatar/dict.h>
#include <travatar/global-debug.h>
#include <travatar/string-util.h>
#include <travatar/hyper-graph.h>
#include <travatar/lookup-table-cfglm.h>
#include <travatar/sentence.h>
#include <travatar/input-file-stream.h>
#include <travatar/lm-func.h>
#include <travatar/vector-hash.h>
#include <travatar/weights.h>
#include <marisa/marisa.h>
#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>
#include <sstream>
#include <fstream>
#include <queue>

using namespace travatar;
using namespace std;
using namespace boost;

// Based on:
//  A CKY+ Variant for SCFG Decoding Without a Dot Chart
//  Rico Sennrich. SSST 2014.

void CFGCollection::AddRules(const CFGPath & path, const RuleVec & rules) {
    boost::shared_ptr<HieroRuleSpans> span(new HieroRuleSpans(path.spans));
    boost::shared_ptr<std::vector<HieroHeadLabels> > label(new std::vector<HieroHeadLabels>(path.labels));
    for(size_t i = 0; i < rules.size(); i++) {
        //cerr << " AddRules: " << *rules[i] << endl;
        rules_.push_back(rules[i]);
        spans_.push_back(span);
        labels_.push_back(label);
    }
}

CFGChartItem::~CFGChartItem() {
    BOOST_FOREACH(StatefulNodeMap::value_type & val, nodes_)
        BOOST_FOREACH(StatefulNode* ptr, val.second)
            delete ptr;
}


Real CFGChartItem::GetHypScore(const HieroHeadLabels & label, int pos) const {
    StatefulNodeMap::const_iterator it = nodes_.find(label);
    assert(it != nodes_.end() && pos >= 0);
    Real ret = ((int)it->second.size() > pos) ? it->second[pos]->first->CalcViterbiScore() : -REAL_MAX;
    return ret;
}


void CFGChartItem::AddStatefulNode(const HieroHeadLabels & label, HyperNode* node, const std::vector<lm::ngram::ChartState> & state) {
    nodes_[label].push_back(new StatefulNode(node, state));
}

const CFGChartItem::StatefulNode & CFGChartItem::GetStatefulNode(const HieroHeadLabels & label, int pos) const {
    StatefulNodeMap::const_iterator it = nodes_.find(label);
    assert(it != nodes_.end() && (int)it->second.size() > pos);
    return *it->second[pos];
}

class StatefulNodeScoresMore {
public:
    bool operator()(const CFGChartItem::StatefulNode* x, const CFGChartItem::StatefulNode* y) {
        return x->first->GetViterbiScore() > y->first->GetViterbiScore();
    }
};

void CFGChartItem::FinalizeNodes() {
    BOOST_FOREACH(StatefulNodeMap::value_type & node_set, nodes_) {
        if(node_set.second.size() > 1)
            sort(node_set.second.begin(), node_set.second.end(), StatefulNodeScoresMore());
    }
    populated_ = true;
}

LookupTableCFGLM::LookupTableCFGLM() : 
      pop_limit_(-1), chart_limit_(-1), trg_factors_(1),
      root_symbol_(HieroHeadLabels(vector<WordId>(GlobalVars::trg_factors+1,Dict::WID("S")))),
      unk_symbol_(HieroHeadLabels(vector<WordId>(GlobalVars::trg_factors+1,Dict::WID("X")))),
      empty_symbol_(HieroHeadLabels(vector<WordId>(GlobalVars::trg_factors+1,Dict::WID("")))),
      weights_(new Weights) { }

LookupTableCFGLM::~LookupTableCFGLM() {
    BOOST_FOREACH(RuleFSM* rule, rule_fsms_) delete rule;
    BOOST_FOREACH(LMData * lm, lm_data_) delete lm;
}

LookupTableCFGLM * LookupTableCFGLM::ReadFromFiles(const std::vector<std::string> & filenames) {
    if(filenames.size() != 1) THROW_ERROR("LookupTableCFGLM currently only supports a single translation model");
    LookupTableCFGLM * ret = new LookupTableCFGLM;
    BOOST_FOREACH(const std::string & filename, filenames) {
        InputFileStream tm_in(filename.c_str());
        //cerr << "Reading TM file from "<<filename<<"..." << endl;
        if(!tm_in)
            THROW_ERROR("Could not find TM: " << filename);
        ret->AddRuleFSM(RuleFSM::ReadFromRuleTable(tm_in));
    }
    return ret;
}

void LookupTableCFGLM::AddRuleFSM(RuleFSM* fsm) {
    rule_fsms_.push_back(fsm);
    BOOST_FOREACH(const UnaryMap::value_type & val, fsm->GetUnaryMap()) {
        Sentence inv_heads(val.first.size());
        for(size_t i = 0; i < val.first.size(); i++) inv_heads[i] = -1 - val.first[i];
        std::string str((char*)&inv_heads[0], inv_heads.size()*sizeof(WordId));
        marisa::Agent ag; ag.set_query(str.c_str(), str.length());
        assert(fsm->GetTrie().lookup(ag));
        BOOST_FOREACH(TranslationRuleHiero * rule, fsm->GetRules()[ag.key().id()]) {
            //cerr << "Adding rule: " << *rule << " at " << val.first << endl;
            unary_ids_[val.first].push_back(unary_rules_.size());
            unary_rules_.push_back(rule);
        }
    }
}

void LookupTableCFGLM::LoadLM(const std::string & filename) {
    lm_data_.push_back(new LMData(filename));
    funcs_.push_back(LMFunc::CreateFromType(lm_data_[lm_data_.size()-1]->GetType())); 
}

bool LookupTableCFGLM::PredictiveSearch(marisa::Agent & agent) const {
    BOOST_FOREACH(const RuleFSM * fsm, rule_fsms_) {
        marisa::Agent tmp_agent; tmp_agent.set_query(agent.query().ptr(), agent.query().length());
        if(fsm->GetTrie().predictive_search(tmp_agent))
            return true;
    }
    return false;
}

void LookupTableCFGLM::Consume(CFGPath & a, const Sentence & sent, int N, int i, int j, int k, vector<CFGChartItem> & chart, vector<CFGCollection> & collections) const {
    //cerr << "Consume(" << CFGPath::PrintAgent(a.agent) << " len==" << a.agent.query().length() << ", " << sent << ", " << N << ", " << i << ", " << j << ", " << k << ")" << endl;
    bool unary = (i == j);
    if(j == k) {
        CFGPath next(a, sent, j);
        if(PredictiveSearch(next.agent))
            AddToChart(next, sent, N, i, k, unary, chart, collections);
    }
    //cerr << " Searching through " << chart[j*N+k].GetNodes().size() << " nodes in chart[" << j*N+k << "]" << endl;
    BOOST_FOREACH(const CFGChartItem::StatefulNodeMap::value_type & sym, chart[j*N+k].GetNodes()) {
        CFGPath next(a, sym.first, j, k);
        //cerr << " Searching for(" << CFGPath::PrintAgent(next.agent) << " len==" << next.agent.query().length() << ")" << endl;
        if(PredictiveSearch(next.agent))
            AddToChart(next, sent, N, i, k, unary, chart, collections);
    }
}

void LookupTableCFGLM::AddToChart(CFGPath & a, const Sentence & sent, int N, int i, int j, bool u, vector<CFGChartItem> & chart, vector<CFGCollection> & collections) const {
    //cerr << "AddToChart(" << CFGPath::PrintAgent(a.agent) << " len==" << a.agent.query().length() << ", " << sent << ", " << N << ", " << i << ", " << j << ", " << u << ")" << endl;
    if(!u) {
        BOOST_FOREACH(const RuleFSM * fsm, rule_fsms_) {
            if(fsm->GetTrie().lookup(a.agent))
                collections[i*N+j].AddRules(a, fsm->GetRules()[a.agent.key().id()]);
        }
    }
    if(PredictiveSearch(a.agent))
        for(int k = j+1; k < N; k++)
            Consume(a, sent, N, i, j+1, k, chart, collections);
}

void LookupTableCFGLM::CubePrune(int N, int i, int j, vector<CFGCollection> & collection, vector<CFGChartItem> & chart, HyperGraph & ret) const {
    //cerr << "CubePrune(" << N << ", " << i << ", " << j << ")" << endl;
    // Don't build already finished charts
    int id = i*N + j;
    assert(!chart[id].IsPopulated());
    // The priority queue of values yet to be expanded
    priority_queue<pair<Real, vector<int> > > hypo_queue;
    // The set indicating already-expanded combinations
    unordered_set<vector<int>, VectorHash<int> > finished;
    // For each rule in the collection, add its best edge to the chart
    const RuleVec & rules = collection[i*N+j].GetRules();
    const CFGCollection::SpanVec & spans = collection[i*N+j].GetSpans();
    const CFGCollection::LabelVec & labels = collection[i*N+j].GetLabels();
    assert(rules.size() == spans.size());

    // Score the top hypotheses of each rule
    //cerr << " Scoring hypotheses for " << rules.size() << " rules" << endl;
    for(size_t rid = 0; rid < rules.size(); rid++) {
        // Get the base score for the rule
        Real score = weights_->GetCurrent() * rules[rid]->GetFeatures();
        const vector<pair<int,int> > & path = *spans[rid];
        const vector<HieroHeadLabels> & lab = *labels[rid];
        assert(lab.size() == path.size());
        for(size_t pid = 0; pid < path.size() && score != -REAL_MAX; pid++) {
            int id = path[pid].first * N + path[pid].second;
            score += chart[id].GetHypScore(lab[pid], 0);
        }
        if(score != -REAL_MAX) {
            vector<int> pos(spans[rid]->size()+1,0); pos[0] = rid;
            hypo_queue.push(make_pair(score, pos));
        }
    }
    
    // Create a map for recombination
    typedef std::pair<HieroHeadLabels, std::vector<lm::ngram::ChartState> > RecombIndex;
    typedef std::map<RecombIndex, HyperNode*> RecombMap;
    RecombMap recomb_map;
    set<vector<int> > finished_hyps;

    // Create the unary path
    vector<pair<int,int> > unary_path(1, pair<int,int>(i,j));

    // Go through the priority queue
    for(int num_popped = 0; hypo_queue.size() != 0 && 
                            (pop_limit_ < 0 || num_popped < pop_limit_) &&
                            (chart_limit_ < 0 || (int)recomb_map.size() < chart_limit_); num_popped++) {
        // Pop the top hypothesis
        Real top_score = hypo_queue.top().first;
        vector<int> id_str = hypo_queue.top().second;
        hypo_queue.pop();
        // Check if this has already been expanded
        if(finished_hyps.find(id_str) != finished_hyps.end()) continue;
        finished_hyps.insert(id_str);
        // Find the paths and rules
        const vector<pair<int,int> > * path;
        const TranslationRuleHiero * rule;
        // Non-unary rules
        if(id_str[0] >= 0) {
            path = spans[id_str[0]].get();
            rule = rules[id_str[0]];
            //cerr << " Non-unary ["<<i<<","<<j+1<<"] (s=" << top_score << "): " << *rule << endl;
        // Unary rules
        } else {
            path = &unary_path;
            rule = unary_rules_[-1-id_str[0]];
            //cerr << " Unary ["<<i<<","<<j+1<<"] (s=" << top_score << "): " << *rule << endl;
        }
        // Create the next edge
        HyperEdge * next_edge = new HyperEdge;
        next_edge->SetFeatures(rule->GetFeatures());
        next_edge->SetTrgData(rule->GetTrgData());
        // next_edge->SetSrcStr(rule.GetSrcStr());
        vector<lm::ngram::ChartState> my_state(lm_data_.size());
        vector<vector<lm::ngram::ChartState> > states(path->size());
        for(size_t pid = 0; pid < path->size(); pid++) {
            const pair<int,int> & my_span = (*path)[pid];
            const CFGChartItem::StatefulNode & node = chart[my_span.first*N + my_span.second].GetStatefulNode(rule->GetChildHeadLabels(pid), id_str[pid+1]);
            next_edge->AddTail(node.first);
            states[pid] = node.second;
        }
        // Calculate the language model score
        Real total_score = 0;
        vector<SparsePair> lm_features;
        for(int lm_id = 0; lm_id < (int)lm_data_.size(); lm_id++) {
            LMData* data = lm_data_[lm_id];
            pair<Real,int> lm_scores = funcs_[lm_id]->CalcNontermScore(data, next_edge->GetTrgData()[data->GetFactor()].words, states, lm_id, my_state[lm_id]);
            // Add to the features and the score
            total_score += lm_scores.first * data->GetWeight() + lm_scores.second * data->GetUnkWeight();
            if(lm_scores.first != 0.0)
                lm_features.push_back(make_pair(data->GetFeatureName(), lm_scores.first));
            if(lm_scores.second != 0)
                lm_features.push_back(make_pair(data->GetUnkFeatureName(), lm_scores.second));
        }
        next_edge->GetFeatures() += SparseVector(lm_features);
        // Add the hypothesis to the hypergraph
        RecombIndex ridx = make_pair(rule->GetHeadLabels(), my_state);
        RecombMap::iterator rit = recomb_map.find(ridx);
        ret.AddEdge(next_edge);
        if(rit != recomb_map.end()) {
            rit->second->AddEdge(next_edge);
            next_edge->SetHead(rit->second);
        } else {
            HyperNode * node = new HyperNode;
            node->SetSpan(make_pair(i, j+1));
            node->SetSym(rule->GetSrcData().label);
            next_edge->SetHead(node);
            ret.AddNode(node);
            chart[id].AddStatefulNode(rule->GetHeadLabels(), node, my_state);
            node->AddEdge(next_edge);
            recomb_map.insert(make_pair(ridx, node));
        }
        // Advance the hypothesis
        for(size_t j = 0; j < path->size(); j++) {
            const pair<int,int> & my_span = (*path)[j];
            Real my_score = top_score + chart[my_span.first*N + my_span.second].GetHypScoreDiff(rule->GetChildHeadLabels(j), id_str[j+1]+1);
            if(my_score > -REAL_MAX/2) {
                vector<int> pos(id_str); pos[j+1]++;
                hypo_queue.push(make_pair(my_score, pos));
            }
        }
        // If unary rules exist
        UnaryIds::const_iterator uit = unary_ids_.find(rule->GetHeadLabels());
        if(uit != unary_ids_.end()) {
            BOOST_FOREACH(int urid, uit->second) {
                vector<int> pos(2); pos[0] = -1-urid; pos[1] = 0;
                // TODO: Recalculating the features for unary rules every time is extremely wasteful. Do something.
                Real my_score = top_score + weights_->GetCurrent() * unary_rules_[urid]->GetFeatures();
                hypo_queue.push(make_pair(my_score, pos));
            }
        }
    }

    // Sort the nodes in each bin, and mark the chart populated
    //cerr << " Finalizing chart[" << id << "]" << endl;
    chart[id].FinalizeNodes();

}

HyperGraph * LookupTableCFGLM::TransformGraph(const HyperGraph & graph) const {

    HyperGraph * ret = new HyperGraph;
    ret->SetWords(graph.GetWords());
       
    Sentence sent = graph.GetWords();
    int N = sent.size();
    vector<CFGChartItem> chart(N*N);
    vector<CFGCollection> collections(N*N);
    CFGPath root_path;

    // Add the root note
    HyperNode * root = new HyperNode;
    root->SetSpan(make_pair(0, N));
    ret->AddNode(root);

    for(int i = N-1; i >= 0; i--) {

        //cerr << "----------------- TOP: i=" << i << endl;
        for(int j = i; j < N; j++) {
            //cerr << "----------------- TOP: j=" << j << endl;
            // Find single words
            if(i == j) {
                CFGPath next(root_path, sent, i);
                BOOST_FOREACH(const RuleFSM * fsm, rule_fsms_) {
                    if(fsm->GetTrie().lookup(next.agent))
                        collections[i*N+j].AddRules(next, fsm->GetRules()[next.agent.key().id()]);
                }
                // if(PredictiveSearch(next.agent))
                //     AddToChart(next, sent, N, i, i, false, chart, collections);
            // Find multi-words
            } else {
                Consume(root_path, sent, N, i, i, j-1, chart, collections);
            }
            CubePrune(N, i, j, collections, chart, *ret);
        }
    }

    // Build the final nodes
    CFGChartItem::StatefulNodeMap::iterator snmit = chart[N-1].GetNodes().find(root_symbol_);
    if(snmit != chart[N-1].GetNodes().end()) {
        BOOST_FOREACH(const CFGChartItem::StatefulNode * sn, snmit->second) {
            HyperEdge * edge = new HyperEdge(root);
            edge->SetTrgData(CfgDataVector(GlobalVars::trg_factors, CfgData(Sentence(1, -1))));
            //cerr << " Adding tail: " << sn->first->GetId() << endl;
            edge->AddTail(sn->first);
            Real total_score = 0;
            for(int lm_id = 0; lm_id < (int)lm_data_.size(); lm_id++) {
                LMData* data = lm_data_[lm_id];
                Real my_score = funcs_[lm_id]->CalcFinalScore(data->GetLM(), sn->second[lm_id]);
                if(my_score != 0.0)
                    edge->GetFeatures().Add(data->GetFeatureName(), my_score);
                total_score += my_score * data->GetWeight();
            }
            edge->SetScore(total_score);
            ret->AddEdge(edge);
            root->AddEdge(edge);
        }
        root->CalcViterbiScore();
    } else {
        delete ret;
        ret = new HyperGraph;
    }

    return ret;
}

