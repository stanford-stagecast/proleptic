#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace std;

constexpr int zero_penalty = 1;
constexpr int length_incentive = 500000; // 50 seconds yields 10% increase in score
constexpr int max_offset = 600;
constexpr int timestamp_max_before_source = 5000; // ms behing the source timestamp
constexpr int min_dist_const = 400;               // acceptable time difference for same note

constexpr int MIN_NOTES = 8;
constexpr int MAX_NOTES = 200;  // Max length of snippet to be calculated
constexpr int MIN_TIME = 1500;  // Min time in ms
constexpr int MAX_TIME = 30000; // If more matches, increase snippet length
constexpr float THRESHOLD = 0.7;
constexpr int START = 191400;
constexpr int SKIP = 10;
constexpr int END = 365000;

//    MIDI EVENT TYPES
//    0x80 (hex)      128 (decimal)    Note Off
//    0x90 (hex)      144 (decimal)    Note On
//    0xB0 (hex)      176 (decimal)    Continuous Controller
struct midi_event
{
  int timestamp;
  // int event_type;
  int note;
  int velocity;
};

struct match
{
  int currTime;
  int target_time;
  float score;
  int source_id_start;
  int source_id_end;
  int target_id_end_lm2;
  int target_id_end;
  int numSourceNotes;
  int sourceTime;
};

int time_to_index( vector<midi_event> notes, int given_timestamp )
{
  // Make binary search
  for ( int i = 0; i < notes.size(); i++ ) {
    if ( notes[i].timestamp >= given_timestamp ) {
      return i;
    }
  }
  return -1;
}

vector<midi_event> midi_to_timeseries( const string& midiPath )
{
  fstream midiFile;
  vector<midi_event> events;
  midiFile.open( midiPath, ios::in );
  // set<int> expected_types = {128, 144, 157};
  if ( midiFile.is_open() ) {
    string txtEvent;
    while ( getline( midiFile, txtEvent ) ) {
      string time, event_type, note, vel;
      stringstream s( txtEvent );
      s >> time >> event_type >> note >> vel;
      if ( stoi( event_type, 0, 16 ) == 144 ) {
        events.emplace_back( stoi( time ), stoi( note, 0, 16 ), stoi( vel, 0, 16 ) );
      }
    }
    midiFile.close(); // close the file object.
  }
  return events;
}

tuple<vector<vector<float>>, float, float> note_similarity_vect2_mean( vector<midi_event> sequence1,
                                                                       vector<midi_event> sequence2,
                                                                       vector<float> ratio )
{
  int min_dist = min_dist_const;
  // vector<float> min_dist = min_dist_const * ratio[", None"];

  int n1 = sequence1.size();
  int n2 = sequence2.size();
  int smaller = n1 < n2 ? n1 : n2;

  vector<vector<int>> time_diffs( n2, vector<int>( n1 ) );
  vector<vector<int>> offset( n2, vector<int>( n1 ) );

  vector<int> best_matching_times( n1, INT_MAX );
  vector<int> best_matching_idxs( n1, INT_MAX );

  vector<int> column_best_matching_idxs( n2 );

  for ( int j = 0; j < n2; j++ ) {
    for ( int i = 0; i < n1; i++ ) {
      time_diffs[j][i] = sequence1[i].timestamp - sequence2[j].timestamp;

      offset[j][i] = ( sequence1[i].note == sequence2[j].note ) * abs( time_diffs[j][i] );
      if ( sequence1[i].note != sequence2[j].note ) {
        offset[j][i] = 100000;
        time_diffs[j][i] = 100000;
      }
      if ( offset[j][i] < best_matching_times[i] ) {
        best_matching_times[i] = offset[j][i];
        best_matching_idxs[i] = j;
      }
    }
    column_best_matching_idxs[j] = distance( offset[j].begin(), min_element( offset[j].begin(), offset[j].end() ) );
  }

  float time_sum_1 = 0;
  float num_under_1 = 0;
  for ( int i = 0; i < n1; i++ ) {
    if ( abs( time_diffs[best_matching_idxs[i]][i] ) < max_offset ) {
      time_sum_1 -= time_diffs[best_matching_idxs[i]][i];
      num_under_1++;
    }
  }
  float offset1 = 0;
  if ( num_under_1 > 0 ) {
    offset1 = time_sum_1 / num_under_1;
  }

  float time_sum_2 = 0;
  float num_under_2 = 0;
  for ( int j = 0; j < n2; j++ ) {
    if ( abs( time_diffs[j][column_best_matching_idxs[j]] ) < max_offset ) {
      time_sum_2 += time_diffs[j][column_best_matching_idxs[j]];
      num_under_2++;
    }
  }
  float offset2 = 0;
  if ( num_under_2 > 0 ) {
    offset2 = time_sum_2 / num_under_2;
  }
  int rounded_offset2 = (int)offset2;
  if ( rounded_offset2 != 0 ) {
    for_each( sequence2.begin(), sequence2.end(), [rounded_offset2]( midi_event& event ) {
      event.timestamp += rounded_offset2;
    } );
  }

  vector<vector<float>> op( sequence2.size(), vector<float>( sequence1.size() ) );
  for ( int j = 0; j < n2; j++ ) {
    for ( int i = 0; i < n1; i++ ) {
      float time_diff = abs( sequence1[i].timestamp - sequence2[j].timestamp );
      op[j][i]
        = ( sequence1[i].note == sequence2[j].note ) * ( time_diff < min_dist ) * ( 1 - time_diff / min_dist );
    }
  }

  return make_tuple( op, offset1, offset2 );
}

tuple<int, int, float, float, float> musical_similarity( vector<midi_event> tf1,
                                                         vector<midi_event> tf2,
                                                         bool disp = false )
{
  vector<midi_event> sequence1 = tf1;
  int last_time_1 = tf1.back().timestamp;
  for ( midi_event& e : sequence1 ) {
    e.timestamp -= last_time_1;
  }

  int last_note = tf1.back().note;
  int ind = -1;
  int lastmatch1 = -1;
  int lastmatch2 = -1;

  for ( size_t i = tf2.size() - 1; i >= 1; i-- ) {
    midi_event e = tf2[i];
    if ( e.note == last_note ) {
      ind = i;
      break;
    }
  }

  int last_time_2 = 0;
  float mo = 0;
  if ( ind != -1 ) {
    lastmatch1 = tf1.size() - 1;
    lastmatch2 = ind;
    mo = tf2.back().timestamp - tf2[ind].timestamp;
    last_time_2 = tf2[ind].timestamp;
  } else {
    last_time_2 = tf2.back().timestamp;
    mo = 0;
  }

  vector<midi_event> sequence2 = tf2;
  for ( midi_event& e : sequence2 ) {
    e.timestamp -= last_time_2;
  }

  int seq_1_time_del = tf1.back().timestamp - tf1[0].timestamp;
  int seq_2_time_del = tf2.back().timestamp - tf2[0].timestamp;

  float time_ratio = ( seq_2_time_del / ( seq_1_time_del * 1.0 ) ) / tf1.size();

  // Calculating score array by comparing every note from each sequence and taking the best match
  vector<float> ratios_arr;
  for ( int i = sequence2.size(); i >= 0; i-- ) {
    ratios_arr.push_back( time_ratio * i );
  }

  float mean_offset1, mean_offset2;
  vector<vector<float>> scores;
  for ( int i = sequence2.size(); i >= 0; i-- ) {
    ratios_arr.push_back( time_ratio * i );
  }

  tie( scores, mean_offset1, mean_offset2 ) = note_similarity_vect2_mean( sequence1, sequence2, ratios_arr );
  std::vector<float> score( sequence1.size(), 0 );
  std::vector<float> score2( sequence2.size(), 0 );
  int last_nonzero_1 = -1;
  int last_nonzero_2 = -1;

  for ( int j = 0; j < static_cast<int>( sequence1.size() ); j++ ) {
    for ( int i = 0; i < static_cast<int>( sequence2.size() ); i++ ) {
      if ( scores[i][j] > score[j] ) {
        score[j] = scores[i][j];
      }
      if ( scores[i][j] > score2[i] ) {
        score2[i] = scores[i][j];
      }
      if ( score[j] != 0 ) {
        last_nonzero_1 = j;
      }
      if ( score2[i] != 0 )
        last_nonzero_2 = i;
    }
  }

  if ( lastmatch1 == -1 ) {
    lastmatch1 = last_nonzero_1;
    lastmatch2 = last_nonzero_2;
  }

  // For every pair of notes in both sequences, if a match was not found, add a zero score
  int count_zeros = 0;
  float score_sum = 0;
  for ( float s : score ) {
    if ( s == 0 )
      count_zeros++;
    score_sum += s;
  }
  count_zeros *= zero_penalty;

  int count_score = sequence2.size() + count_zeros;

  // Similarity of the two sequences is the mean of the note similarity scores
  float score_1 = score_sum / ( count_score * 1.0 );

  // including length in score, if at least 5 notes
  if ( tf1.size() > 5 ) {
    score_1 += score_1 * seq_1_time_del / length_incentive; // 50 seconds yield 10% increase
  }

  if ( disp ) {
    // TODO: Draw graph
  }

  return make_tuple( lastmatch1, lastmatch2, mean_offset1 + mo, mean_offset2 + mo, score_1 );
}

tuple<int, int, float, float, float> two_way_similarity( vector<midi_event> tf1,
                                                         vector<midi_event> tf2,
                                                         bool disp = false )
{
  int a1, b1, a2, b2;
  float c1, d1, c2, d2, score1, score2;
  tie( a1, b1, c1, d1, score1 ) = musical_similarity( tf1, tf2, disp );
  tie( b2, a2, d2, c2, score2 ) = musical_similarity( tf2, tf1, disp );

  if ( score1 > score2 ) {
    return { a1, b1, c1, d1, score1 };
  }
  return { a2, b2, c2, d2, score2 };
}

vector<match> calculate_similarity_time( vector<midi_event> notes,
                                         vector<int> source_id,
                                         int currTime,
                                         bool disp = false,
                                         int skip = 100 )
/* Function that calls musical similarity on targets generated for a source_id.
        Target snips start at every 100 ms, and has same time length as source.

    Args:
        notes: list of all notes from a recording [[t,note,vel],[t,note,vel],[t,note,vel],...]
        source_id: indices of note array corresponding to current time snippet (source_id_start>source_id_end)
                  [source_id_start, source_id_end]
        currTime: time stamp at which we are searching for matches (ms)
        skip: interval at which to iterate over target timestamps
        disp: boolean whether to print each match (defaults True)

    Returns:
        matches: list of matches [[currTime, pastTime1, score1], [currTime, pastTime2, score2],...]

*/
{
  vector<match> matches;
  vector<int> max_matches;
  int last_id_end = 0; // track previous end index of target
  int last_id_start = 0;
  int source_id_start = source_id[0];
  int source_id_end = source_id[1];
  int source_end = notes[source_id_end].timestamp; // start and end time stamps of source
  int length_ms = currTime - source_end;

  int target_start = length_ms;
  int target_id_end = 0;
  int target_id_start = 0;
  while ( target_start < currTime - timestamp_max_before_source ) {
    // pick target_end by time length of course snip
    int target_end = target_start - length_ms;

    // Finding new end index
    for ( int i = last_id_end; i < static_cast<int>( notes.size() ); i++ ) {
      if ( notes[i].timestamp >= target_end ) {
        target_id_end = i;
        break;
      }
    }

    // Finding new start index
    for ( int i = target_id_end; i < static_cast<int>( notes.size() ); i++ ) {
      if ( notes[i].timestamp >= target_start ) {
        target_id_start = i;
        break;
      }
    }

    if ( target_id_end > target_id_start - 4 ) {
      last_id_end = target_id_end;
      last_id_start = target_id_start;
      target_start += skip;
      continue;
    }
    if ( target_id_end == last_id_end && last_id_start == target_id_start ) {
      target_start += skip;
      continue;
    }

    int lm1, lm2;
    float mo1, mo2, score;
    tie( lm1, lm2, mo1, mo2, score )
      = two_way_similarity( vector<midi_event>( notes.begin() + source_id_end, notes.begin() + source_id_start ),
                            vector<midi_event>( notes.begin() + target_id_end, notes.begin() + target_id_start ),
                            false );

    if ( score > 0.7 ) {
      if ( disp ) {
        // TODO: Add graph
      }
    }
    if ( score > 0.5 ) {
      int target_time = target_start;
      if ( lm1 >= source_id_start - source_id_end - 2 ) {
        target_time
          = notes[target_id_start - 1].timestamp + (int)mo2 + ( currTime - notes[source_id_start - 1].timestamp );
      } else if ( lm2 >= target_id_start - target_id_end - 2 ) {
        if ( currTime - notes[source_id_end + lm1].timestamp < 1 ) {
          cout << "oops something went wrong with time calculations - might end in infinite loop" << endl;
        }
        target_start += currTime - notes[source_id_end + lm1].timestamp;
        continue;
      }

      target_time
        = notes[target_id_start - 1].timestamp - (int)mo2 + ( currTime - notes[source_id_start - 1].timestamp );
      if ( target_time < currTime - timestamp_max_before_source ) {
        matches.emplace_back( currTime,
                              target_time,
                              score,
                              source_id_start,
                              source_id_end,
                              time_to_index( notes, target_time ),
                              target_id_end,
                              0,
                              0 );
      }
    }
    last_id_end = target_id_end;
    last_id_start = target_id_start;
    target_start += skip;
  }
  return matches;
}

vector<int> get_source_notes( vector<midi_event> notes, int start_time, int min_time )
/*
Args:
        notes: array of all notes in a recording, where each note is [t,note,vel]
        start_time: start index of notes array - corresponding to current time
        min_notes: min number of notes for a valid sequence, integer
        max_notes: max note length for a snippet, integer
        min_time: min time length for a valid sequence, integer (ms)\

    Returns:
        matches: list of matches [[currTime, pastTime1, score1], [currTime, pastTime2, score2],...]
*/
{
  int start_index = -1;
  vector<int> source_id;
  for ( size_t i = 0; i < notes.size(); i++ ) {
    if ( notes[i].timestamp > start_time ) {
      start_index = i;
      break;
    }
  }
  if ( start_index < MIN_NOTES )
    return source_id;

  vector<int> end_index;
  for ( int i = start_index - MIN_NOTES; i > start_index - MAX_NOTES - 1; i-- ) {
    int idx = i;
    if ( idx < 0 ) {
      idx = notes.size() + idx;
    }
    end_index.push_back( idx );
  }

  vector<int> ids;
  for ( size_t i = 0; i < end_index.size(); i++ ) {
    if ( start_time - notes[end_index[i]].timestamp >= min_time && end_index[i] != 0 ) {
      ids.push_back( end_index[i] );
    }
  }

  if ( !ids.empty() ) {
    source_id.push_back( start_index );
    source_id.push_back( ids[0] );
  }
  return source_id;
}

vector<match> find_matches_at_timestamp( int i, vector<midi_event> notes, bool disp )
/*
    Args:
        i:
        notes:
        minNotes:
        minTime:
        maxNotes:
        maxTime:
        thresh:
        disp:

    Returns:
        sims_arr: np array of every match >0.5 found of the form -
            ['source_timestamp', 'target_timestamp','score',
            'source_id_start','source_id_end','target_id_start','target_id_end','match_len','match_time']
*/
{
  vector<match> sims_arr;
  int offset = 500;
  int numSourceNotes = 0;
  int sourceTime = 0;
  while ( sourceTime < MAX_TIME ) {
    vector<int> sourceId = get_source_notes( notes, i, sourceTime + offset );

    if ( sourceId.size() >= 2 ) {
      numSourceNotes = sourceId[0] - sourceId[1];
      sourceTime = i - notes[sourceId[1]].timestamp;

      vector<match> sim = calculate_similarity_time( notes, sourceId, i, disp );

      for ( int j = 0; j < static_cast<int>( sim.size() ); j++ ) {
        sim[j].numSourceNotes = numSourceNotes;
        sim[j].sourceTime = sourceTime;
      }
      sims_arr.insert( sims_arr.end(), sim.begin(), sim.end() );
      offset += 500;
    } else {
      break;
    }
  }
  return sims_arr;
}

void program_body( const string& midiPath, const string& outputPath, const string& paramPath )
{
  vector<midi_event> events = midi_to_timeseries( midiPath );
  vector<int> sims_arr;
  ofstream outputParam;
  outputParam.open( paramPath );

  outputParam << "{\"start\": \"" << START << "\", \"end\": \"" << END << "\", \"skip\": \"" << SKIP
              << "\", \"thresh\": \"" << THRESHOLD << "\", \"minNotes\": \"" << MIN_NOTES << "\", \"maxNotes\": \""
              << MAX_NOTES << "\"}";

  outputParam.close();

  ofstream outputCSV;
  outputCSV.open( outputPath );
  outputCSV << "source_timestamp,target_timestamp,score\n";

  for ( int i = START; i < END; i += SKIP ) {
    vector<match> matches = find_matches_at_timestamp( i, events, false );

    for ( match m : matches ) {
      outputCSV << to_string( m.currTime ) + "," + to_string( m.target_time ) + "," + to_string( m.score ) + "\n";
    }
  }
  outputCSV.close();
}

void usage_message( const string_view argv0 )
{
  cerr << "Usage: " << argv0 << " [MIDI file] [Match Output CSV Name] [Paramaters Output Path]\n";
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 4 ) {
      usage_message( argv[0] );
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2], argv[3] );

  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}