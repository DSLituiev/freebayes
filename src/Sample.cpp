#include "Sample.h"


// sample tracking and allele sorting
// the number of observations for this allele
int Sample::observationCount(Allele& allele) {
    return observationCount(allele.currentBase);
}

// the number of observations for this base
int Sample::observationCount(const string& base) {
    Sample::iterator g = find(base);
    if (g != end())
        return g->second.size();
    else
        return 0;
}

// the total number of observations
int Sample::observationCount(void) {
    int count = 0;
    for (Sample::iterator g = begin(); g != end(); ++g) {
        count += g->second.size();
    }
    return count;
}

// puts alleles into the right bins if they have changed their base (as
// occurs in the case of reference alleles)
void Sample::sortReferenceAlleles(void) {

    for (Sample::iterator group = begin(); group != end(); ++group) {

        const string& groupBase = group->first;
        vector<Allele*>& alleles = group->second;

        if (alleles.empty())
            continue;

        const string& base = alleles.front()->currentBase;

        if (base != groupBase && alleles.front()->type == ALLELE_REFERENCE) {
            Sample::iterator g = find(base);
            if (g != end()) {
                vector<Allele*> newgroup = g->second;
                newgroup.reserve(newgroup.size() + distance(alleles.begin(), alleles.end()));
                newgroup.insert(newgroup.end(), alleles.begin(), alleles.end());
            } else {
                insert(begin(), make_pair(base, alleles));
            }
            alleles.clear();
        } else {
            if (!(alleles.front()->type == ALLELE_REFERENCE) && base != groupBase) {
                cerr << alleles.front() << endl;
            }
        }
    }
}

pair<pair<int, int>, pair<int, int> >
Sample::baseCount(string refbase, string altbase) {

    int forwardRef = 0;
    int reverseRef = 0;
    int forwardAlt = 0;
    int reverseAlt = 0;

    for (Sample::iterator s = begin(); s != end(); ++s) {
        vector<Allele*>& alleles = s->second;
        for (vector<Allele*>::iterator a = alleles.begin(); a != alleles.end(); ++a) {
            string base = (*a)->currentBase;
            AlleleStrand strand = (*a)->strand;
            if (base == refbase) {
                if (strand == STRAND_FORWARD)
                    ++forwardRef;
                else if (strand == STRAND_REVERSE)
                    ++reverseRef;
            } else if (base == altbase) {
                if (strand == STRAND_FORWARD)
                    ++forwardAlt;
                else if (strand == STRAND_REVERSE)
                    ++reverseAlt;
            }
        }
    }

    return make_pair(make_pair(forwardRef, forwardAlt), make_pair(reverseRef, reverseAlt));

}

vector<Genotype*> Sample::genotypesWithEvidence(vector<Genotype>& genotypes) {
    vector<Genotype*> supportedGenotypes;
    for (vector<Genotype>::iterator g = genotypes.begin(); g != genotypes.end(); ++g) {
        Genotype& genotype = *g;
        for (map<string, int>::iterator c = genotype.alleleCounts.begin(); c != genotype.alleleCounts.end(); ++c) {
            if (this->find(c->first) != this->end()) {
                supportedGenotypes.push_back(&genotype);
                break;
            }
        }
    }
    return supportedGenotypes;
}

int Sample::baseCount(string base, AlleleStrand strand) {

    int count = 0;
    for (Sample::iterator g = begin(); g != end(); ++g) {
        vector<Allele*>& alleles = g->second;
        for (vector<Allele*>::iterator a = alleles.begin(); a != alleles.end(); ++a) {
            if ((*a)->currentBase == base && (*a)->strand == strand)
                ++count;
        }
    }
    return count;

}


string Sample::json(void) {
    stringstream out;
    out << "[";
    bool first = true;
    for (map<string, vector<Allele*> >::iterator g = this->begin(); g != this->end(); ++g) {
        vector<Allele*>& alleles = g->second;
        for (vector<Allele*>::iterator a = alleles.begin(); a != alleles.end(); ++a) {
            if (!first) { out << ","; } else { first = false; }
            out << (*a)->json();
        }
    }
    out << "]";
    return out.str();
}

vector<int> Sample::alleleObservationCounts(Genotype& genotype) {
    vector<int> counts;
    for (Genotype::iterator i = genotype.begin(); i != genotype.end(); ++i) {
        int count = 0;
        Allele& b = i->allele;
        counts.push_back(observationCount(b));
    }
    return counts;
}

void groupAlleles(Samples& samples, map<string, vector<Allele*> >& alleleGroups) {
    for (Samples::iterator s = samples.begin(); s != samples.end(); ++s) {
        Sample& sample = s->second;
        for (Sample::iterator g = sample.begin(); g != sample.end(); ++g) {
            const string& base = g->first;
            const vector<Allele*>& alleles = g->second;
            vector<Allele*>& group = alleleGroups[base];
            group.reserve(group.size() + distance(alleles.begin(), alleles.end()));
            group.insert(group.end(), alleles.begin(), alleles.end());
        }
    }
}


bool sufficientAlternateObservations(Samples& samples, int mincount, float minfraction) {

    int totalAlternateCount = 0;
    int totalReferenceCount = 0;

    for (Samples::iterator s = samples.begin(); s != samples.end(); ++s) {

        //cerr << s->first << endl;
        Sample& sample = s->second;
        int alternateCount = 0;
        int observationCount = 0;

        for (Sample::iterator group = sample.begin(); group != sample.end(); ++group) {
            const string& base = group->first;
            //cerr << base << endl;
            vector<Allele*>& alleles = group->second;
            //cerr << alleles.size() << endl;
            if (alleles.size() == 0)
                continue;
            if (alleles.front()->type != ALLELE_REFERENCE) {
                alternateCount += alleles.size();
            } else {
                totalReferenceCount += alleles.size();
            }
            observationCount += alleles.size();
        }

        if (alternateCount >= mincount && ((float) alternateCount / (float) observationCount) >= minfraction)
            return true;
        totalAlternateCount += alternateCount;
    
    }

    // always analyze if we have more alternate observations than reference observations
    // this is meant to catch the case in which the reference is the rare allele
    // it will probably also catch cases in which we have very low coverage
    if (totalReferenceCount < totalAlternateCount) {
        return true;
    }

    return false;

}


int countAlleles(Samples& samples) {

    int count = 0;
    for (Samples::iterator s = samples.begin(); s != samples.end(); ++s) {
        Sample& sample = s->second;
        for (Sample::iterator sg = sample.begin(); sg != sample.end(); ++sg) {
            count += sg->second.size();
        }
    }
    return count;

}
