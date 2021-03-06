#ifndef LOOKUP_TABLE_H__
#define LOOKUP_TABLE_H__

#include <travatar/graph-transformer.h>
#include <travatar/sparse-map.h>
#include <travatar/translation-rule.h>
#include <boost/shared_ptr.hpp>
#include <vector>
#include <map>
#include <climits>

namespace travatar {

class HyperNode;

// A single state for a partial rule match
// This must be overloaded with a state that is used in a specific implementation
class LookupState {
public:
    LookupState() { }
    virtual ~LookupState() { }

    const std::vector<const HyperNode*> & GetNonterms() const { return nonterm_nodes_; }
    std::vector<const HyperNode*> & GetNonterms() { return nonterm_nodes_; }
    void SetNonterms(const std::vector<const HyperNode*> & nonterm_nodes) { nonterm_nodes_ = nonterm_nodes; }
    const SparseVector & GetFeatures() const { return features_; }
    void SetFeatures(const SparseVector & feat) { features_ = feat; }
    void AddFeatures(const SparseVector & feat) { features_ += feat; }
    const std::string & GetString() const { return curr_string_; }
    void SetString(const std::string & str) { curr_string_ = str; }

protected:
    // Links to the nodes of non-terminals that are abstracted
    std::vector<const HyperNode*> nonterm_nodes_;
    SparseVector features_;
    // A string representing the current progress
    std::string curr_string_;
};

typedef std::pair<std::vector<boost::shared_ptr<LookupState> >, const HyperNode*> SpannedState;

// Class to compare State, basically comparison is based on the span length.
// The shorter comes first.
class SpannedStateComparator {
public:
    bool operator() (const SpannedState& lhs, const SpannedState& rhs) const;
};

// An implementation of a GraphTransformer that takes as input
// a parse forest of the original sentence, looks up rules, and
// outputs a rule graph in the target language
class LookupTable : public GraphTransformer {
public:
    LookupTable();
    virtual ~LookupTable();

    virtual HyperGraph * TransformGraph(const HyperGraph & parse) const;

    // Find all the translation rules rooted at a particular node in a parse graph
    std::vector<boost::shared_ptr<LookupState> > LookupSrc(
            const HyperNode & node, 
            const std::vector<boost::shared_ptr<LookupState> > & old_states) const;

    // Find rules associated with a particular source pattern
    virtual const std::vector<TranslationRule*> * FindRules(const LookupState & state) const = 0;

    // Get the unknown rule
    const TranslationRule * GetUnknownRule() const;

    virtual LookupState * GetInitialState() const = 0;

    // Accessors
    void SetMatchAllUnk(bool match_all_unk) { match_all_unk_ = match_all_unk; }
    bool GetMatchAllUnk() { return match_all_unk_; }
    void SetSaveSrcStr(bool save_src_str) { save_src_str_ = save_src_str; }
    bool GetSaveSrcStr() { return save_src_str_; }
    void SetConsiderTrg(bool consider_trg) { consider_trg_ = consider_trg; }
    bool GetConsiderTrg() { return consider_trg_; } 
protected:

    // Match a single node
    // For example S(NP(PRN("he")) x0:VP) will match for "he" and VP
    // If matching a non-terminal (e.g. VP), advance the state and push "node"
    // on to the list of non-terminals. Otherwise, just advance the state
    // Returns NULL if no rules were matched
    virtual LookupState * MatchNode(const HyperNode & node, const LookupState & state) const = 0;

    // Match the start of an edge
    // For example S(NP(PRN("he")) x0:VP) will match the opening bracket 
    // of S( or NP( or PRN(
    virtual LookupState * MatchStart(const HyperNode & node, const LookupState & state) const = 0;
    
    // Match the end of an edge
    // For example S(NP(PRN("he")) x0:VP) will match the closing brackets for
    // of (S...) or (NP...) or (PRN...)
    virtual LookupState * MatchEnd(const HyperNode & node, const LookupState & state) const = 0;

    TranslationRule unk_rule_;
    // Match all nodes with the unknown rule, not just when no other rule is matched (default false)
    bool match_all_unk_;
    // Save the source string in the graph or not (default false)
    bool save_src_str_;
    // Whether to consider target side head or not
    bool consider_trg_;

    // Basic Transform Graph
    virtual HyperGraph * TransformGraphSrc(const HyperGraph & parse) const;
    virtual HyperGraph * TransformGraphSrcTrg(const HyperGraph & parse) const;
};

}

#endif
