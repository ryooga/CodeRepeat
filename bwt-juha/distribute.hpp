/*
 * distribute.hpp
 *
 * Copyright 2004 Juha K"arkk"ainen <juha.karkkainen@cs.helsinki.fi>
 *
 */

#ifndef DISTRIBUTE_HPP
#define DISTRIBUTE_HPP

#include "dcsuffixsort.hpp"

#include <iterator>
#include <algorithm>
#include <utility>


//----------------------------------------------------------------------
// suffix_lcp
//
// Compute the length of the longest common prefix between two
// suffixes a and b. 
// str_end = end of the text
// minlcp = length of the already known common prefix
// maxlcp = stop when the length reaches maxlcp
//----------------------------------------------------------------------
template <class StringIterator>
typename std::iterator_traits<StringIterator>::difference_type
suffix_lcp(StringIterator a, StringIterator b, StringIterator str_end,
	   typename std::iterator_traits<StringIterator>::difference_type minlcp,
	   typename std::iterator_traits<StringIterator>::difference_type maxlcp)
//	   LcpType minlcp, LcpType maxlcp)
{
  if (a < b) swap(a,b);   // make a the shorter suffix
  if ((str_end-a) > maxlcp) str_end = a + maxlcp;
  std::pair<StringIterator,StringIterator> mm =
	      std::mismatch(a+minlcp, str_end, b+minlcp);
  //std::cout << " lcp=" << mm.first-a << "\n";
  return mm.first-a;
}


//----------------------------------------------------------------------
// class suffix_distributor
//----------------------------------------------------------------------
template <class StringIterator, class SuffixIterator,
	  class DCSampler, class RankIterator>
class suffix_distributor
{
private:
  //typedef typename std::iterator_traits<StringIterator>::difference_type 
  //  lcptype; 
  typedef short lcptype;
  typedef typename 
    std::iterator_traits<SuffixIterator>::difference_type difftype; 


  StringIterator str, str_end;
  SuffixIterator pivots, pivots_end;
  std::vector<lcptype> lcps;
  const DCSampler& dcsampler;
  RankIterator dcranks;
  lcptype maxlcp;

  typedef typename std::vector<lcptype>::iterator lcpiterator;

public:

  suffix_distributor(StringIterator str_, StringIterator str_end_,
		     SuffixIterator pivots_, SuffixIterator pivots_end_,
		     const DCSampler& dcsampler_, RankIterator dcranks_)
    : str(str_), str_end(str_end_), pivots(pivots_), pivots_end(pivots_end_),
      lcps(pivots_end-pivots), dcsampler(dcsampler_), dcranks(dcranks_),
    maxlcp(dcsampler.period())
  {
    compute_lcps();
  }

  void reset(SuffixIterator pivots_, SuffixIterator pivots_end_)
  {
    pivots=pivots_; 
    pivots_end=pivots_end_;
    lcps.resize(pivots_end-pivots);

    compute_lcps();
  }
  

  difftype nbuckets() const{ return pivots_end-pivots+1; }

  difftype find_bucket(StringIterator suf) const {
    return binary_search(suf);
    //return i - pivots;
  }

  void
  print() const {
#if DEBUG > 1
    std::cout << "distributor memory=" 
	      << lcps.size()*sizeof(lcptype)
	      << std::endl;
#if DEBUG > 2
    std::cout << "pivots and lcps:\n";
    for (SuffixIterator i = pivots; i != pivots_end; ++i) {
      std::cout << std::setw(4) << i-pivots
		<< std::setw(4) << int(lcps[i-pivots])
		<< std::setw(9) << *i << " ";
      std::copy(str+*i, std::min(str_end, str+*i+40),
		std::ostream_iterator<char>(std::cout,""));
      std::cout << "\n";
    }
    std::cout << "distributor end\n";
#endif
#endif
  }


private:

  bool dc_less_or_equal(difftype a, difftype b) const {
    std::pair<difftype,difftype> rep = dcsampler.pair(a, b);
    return dcranks[rep.first] <= dcranks[rep.second];
  }

  //----------------------------------------------------------------------
  // compute_lcps_bounded
  // 
  // Compute all the lcp values in a subrange bounded by
  // actual pivot suffixes.
  //----------------------------------------------------------------------
  void
  compute_lcps_bounded(SuffixIterator left_suf, SuffixIterator right_suf,
		      lcpiterator left_lcp, 
		      lcptype minlcp, lcptype maxlcp)
  {
    if (left_suf == right_suf) return;
    difftype midpoint = (right_suf-left_suf)/2;
    //std::cout << "  midpoint of [0," << right_suf-left_suf 
    //          << ")=" << midpoint << "\n";
    
    SuffixIterator mid_suf = left_suf + midpoint;
    lcpiterator mid_lcp = left_lcp + midpoint;
    
    lcptype llcp_left = suffix_lcp(str+*(left_suf-1), str+*mid_suf, 
				   str_end, minlcp, maxlcp);
    lcptype llcp_right = suffix_lcp(str+*mid_suf, str+*right_suf, 
				    str_end, minlcp, maxlcp);
    *mid_lcp = llcp_right - llcp_left;
    //std::cout << *mid_lcp << std::endl;

    compute_lcps_bounded(left_suf, mid_suf, left_lcp, llcp_left, maxlcp);
    compute_lcps_bounded(mid_suf+1, right_suf, mid_lcp+1, llcp_right, maxlcp);
  }
  
  
  //----------------------------------------------------------------------
  // compute_lcps
  //
  // Compute all the lcp values in the full range.
  //----------------------------------------------------------------------
  void
  compute_lcps()
  {
    if (pivots == pivots_end) return;
    difftype centerpoint = (pivots_end - pivots)/2;
    //std::cout << "midpoint of [0," << (pivots_end - pivots) << ")="
    //      << centerpoint << "\n";
    lcps[centerpoint] = 0;
    
    // unbounded ranges on the left
    difftype rightpoint = centerpoint;
    while (rightpoint > 0) {
      difftype midpoint = rightpoint/2;
      //std::cout << "midpoint of [0," << rightpoint << ")=" << midpoint << "\n";
      lcptype llcp = suffix_lcp(str+pivots[midpoint], 
			       str+pivots[rightpoint], 
			       str_end, static_cast<lcptype>(0), maxlcp);
      lcps[midpoint] = llcp;
      //std::cout << llcp << std::endl;
      compute_lcps_bounded(pivots+midpoint+1, pivots+rightpoint, 
			   lcps.begin()+midpoint+1, llcp, maxlcp);
      rightpoint = midpoint;
    }
    
    // unbounded ranges on the right
    SuffixIterator left_suf = pivots + centerpoint + 1;
    lcpiterator left_lcp = lcps.begin() + centerpoint + 1;
    while (left_suf != pivots_end) {
      difftype midpoint = (pivots_end - left_suf)/2;
      //std::cout << "midpoint of [" << (left_suf-pivots) << "," << 
      //  (pivots_end-pivots) << ")=" << (left_suf-pivots)+midpoint << "\n";
      lcptype llcp = suffix_lcp(str+left_suf[-1], str+left_suf[midpoint], 
			       str_end, static_cast<lcptype>(0), maxlcp);
      left_lcp[midpoint] = -llcp;
      //std::cout << -llcp << std::endl;
      compute_lcps_bounded(left_suf, left_suf+midpoint, left_lcp, 
			   llcp, maxlcp);
      left_suf += midpoint + 1;
      left_lcp += midpoint + 1;
    }
    
  }
  


#ifndef BINSEARCH
  
//----------------------------------------------------------------------
// binary_search
//----------------------------------------------------------------------
difftype
//SuffixIterator
binary_search(StringIterator query) const
{
  //SuffixIterator left = pivots;
  //SuffixIterator right = pivots_end;
  //SuffixIterator mid;

  difftype left = 0;
  difftype right = pivots_end-pivots;
  difftype mid;

  lcptype minlcp = 0;
  lcptype lcp_diff = 0;
  lcptype lcp_left, lcp_right;

 in_the_middle:
  while (true) {
    /*
    std::cout << "middle minlcp=" << int(minlcp)
	      << " lcp_diff=" << int(lcp_diff)
	      << " mid=" << left + (right-left)/2
	      << "\n";
    */
    if (left == right) return left;
    mid = left + (right-left)/2;
    if (str+pivots[mid] == query) return mid+1;
    lcptype lcp_mid = lcps[mid];
    if (lcp_mid == 0) {
      lcptype newlcp = suffix_lcp(query, str+pivots[mid], str_end, 
				  minlcp, maxlcp);
      if (newlcp == maxlcp) {
	lcp_left = lcp_right = minlcp;
	goto fullmatch;
      }
      if (query+newlcp == str_end ||
	  query[newlcp] < str[pivots[mid]+newlcp]) {
	right = mid;
	lcp_diff = newlcp - minlcp;
	if (lcp_diff != 0) goto closer_to_right;
      } else {
	left = mid+1;
	lcp_diff = minlcp - newlcp;
	if (lcp_diff != 0) goto closer_to_left;
      }
    } else {
      if (lcp_mid < lcp_diff) {
	left = mid+1;
      } else {
	right = mid;
      }
    }
  }

 closer_to_left:
  while (true) {
    /*
    std::cout << "left minlcp=" << int(minlcp)
	      << " lcp_diff=" << int(lcp_diff)
	      << " mid=" << left + (right-left)/2
	      << "\n";
    */
    if (left == right) return left;
    mid = left + (right-left)/2;
    if (str+pivots[mid] == query) return mid+1;
    lcptype lcp_mid = lcps[mid];
    if (lcp_mid > lcp_diff) {
      if (lcp_mid < 0) {
	lcp_diff -= lcp_mid;
	minlcp -= lcp_mid;
      }
      right = mid;
    } else {
      if (lcp_mid == lcp_diff) {
	lcptype lcp = minlcp - lcp_diff;
	lcptype newlcp = suffix_lcp(query, str+pivots[mid], str_end, 
				    lcp, maxlcp);
	if (newlcp == maxlcp) {
	  lcp_left = lcp;
	  lcp_right = minlcp;
	  goto fullmatch;
	}
	if (query+newlcp == str_end ||
	    query[newlcp] < str[pivots[mid]+newlcp]) {
	  right = mid;
	  minlcp = lcp;
	  lcp_diff = newlcp - minlcp;
	  if (lcp_diff == 0) goto in_the_middle;
	  else goto closer_to_right;
	} else {
	  left = mid+1;
	  lcp_diff = minlcp - newlcp;
	}
      } else {
	left = mid+1;
      }
    }
  }

 closer_to_right:
  while (true) {
    /*
    std::cout << "right minlcp=" << int(minlcp)
	      << " lcp_diff=" << int(lcp_diff)
	      << " mid=" << left + (right-left)/2
	      << "\n";
    */
    if (left == right) return left;
    mid = left + (right-left)/2;
    if (str+pivots[mid] == query) return mid+1;
    lcptype lcp_mid = lcps[mid];
    if (lcp_mid < lcp_diff) {
      if (lcp_mid > 0) {
	lcp_diff -= lcp_mid;
	minlcp += lcp_mid;
      }
      left = mid+1;
    } else {
      if (lcp_mid == lcp_diff) {
	lcptype lcp = minlcp + lcp_diff;
	lcptype newlcp = suffix_lcp(query, str+pivots[mid], str_end, 
				    lcp, maxlcp);
	if (newlcp == maxlcp) {
	  lcp_left = minlcp;
	  lcp_right = lcp;
	  goto fullmatch;
	}
	if (query+newlcp == str_end ||
	    query[newlcp] < str[pivots[mid]+newlcp]) {
	  right = mid;
	  lcp_diff = newlcp - minlcp;
	} else {
	  left = mid+1;
	  minlcp = lcp;
	  lcp_diff = minlcp - newlcp;
	  if (lcp_diff == 0) goto in_the_middle;
	  else goto closer_to_left;
	}
      }
      right = mid;
    }
  }

 fullmatch:
  // find the range of fully matching pivots
  //std::cout << "fullmatch: left\n";
  {
    difftype right_tmp = mid;
    while (left != right_tmp) {
      difftype mid = left + (right_tmp-left)/2;
      //std::cout << left << mid << right_tmp << std::endl;
      difftype lcp_diff = lcps[mid];
      //std::cout << " " << lcp_left << " " << lcp_diff << std::endl;
      if (lcp_left + lcp_diff == maxlcp) {
	right_tmp = mid;
      } else {
	if (lcp_diff > 0) { lcp_left += lcp_diff; }
	left = mid+1;
      }
    }
  }
  //std::cout << "fullmatch: right\n";
  {
    difftype left_tmp = mid+1;
    while (left_tmp != right) {
      difftype mid = left_tmp + (right-left_tmp)/2;
      //std::cout << left_tmp << mid << right << std::endl;
      difftype lcp_diff = lcps[mid];
      //std::cout << " " << lcp_right << " " << lcp_diff << std::endl;
      if (lcp_right - lcp_diff == maxlcp) {
        left_tmp = mid+1;
      } else {
	if (lcp_diff < 0) { lcp_right -= lcp_diff; }
	right = mid;
      }
    }
  }
  //std::cout << "fullmatch: dcsearch\n";
  while (left != right) {
    difftype mid = left + (right-left)/2;
    //std::cout << left << mid << right << std::endl;
    if (dc_less_or_equal(pivots[mid], query-str)) {
      left = mid+1;
    } else {
      right = mid;
    }
  }
  return left;
  
}

#endif

};


#endif // DISTRIBUTE_HPP
