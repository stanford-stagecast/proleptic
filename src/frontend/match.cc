#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace std;

constexpr int MIN_NOTES = 8;
constexpr int MAX_NOTES = 200;  // Max length of snippet to be calculated
constexpr int MIN_TIME = 1500;  // Min time in ms
constexpr int MAX_TIME = 30000; // If more matches, increase snippet length
constexpr float THRESHOLD = 0.7;
constexpr int START = 191400;
constexpr int SKIP = 500;
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

vector<vector<float>> note_similarity_vect2( vector<midi_event> sequence1,
                                             vector<midi_event> sequence2,
                                             vector<float> ratio )
{
  // score is linear with time difference between notes
  vector<vector<float>> op( sequence2.size(), vector<float>( sequence1.size() ) );
  float min_dist = 50 * ratio[0]; // acceptable time difference for same note

  for ( int i = 0; i < static_cast<int>( sequence2.size() ); i++ ) {
    for ( int j = 0; j < static_cast<int>( sequence1.size() ); j++ ) {
      float time_diff = abs( sequence1[j].timestamp - sequence2[i].timestamp );
      // remapping time diff to 0.5-1
      op[i][j]
        = ( sequence1[j].note == sequence2[i].note ) * ( time_diff < min_dist ) * ( 1 - time_diff / min_dist );
    }
  }
  return op;
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

  int last_note = tf2.back().note;
  int ind = -1;
  int lastmatch1 = -1;
  int lastmatch2 = -1;

  for ( size_t i = 0; i < tf2.size(); i++ ) {
    midi_event e = tf2[i];
    if ( e.note == last_note ) {
      ind = i;
    }
  }

  int last_time_2 = 0;
  if ( ind != -1 ) {
    lastmatch1 = tf1.size() - 1;
    lastmatch2 = ind;
    last_time_2 = tf2[ind].timestamp;
  } else {
    last_time_2 = tf2.back().timestamp;
  }

  vector<midi_event> sequence2 = tf2;
  for ( midi_event& e : sequence2 ) {
    e.timestamp -= last_time_2;
  }

  int seq_1_time_del = tf1.back().timestamp - tf1[0].timestamp;
  int seq_2_time_del = tf2.back().timestamp - tf2[0].timestamp;

  float time_ratio = seq_2_time_del / ( seq_1_time_del * 1.0 );

  // Calculating score array by comparing every note from each sequence and taking the best match
  vector<float> ratios_arr;
  for ( int i = sequence2.size(); i >= 0; i-- ) {
    ratios_arr.push_back( time_ratio * i );
  }
  vector<vector<float>> scores = note_similarity_vect2( sequence1, sequence2, ratios_arr );
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
      if ( score[j] != 0 )
        last_nonzero_1 = j;
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

  int count_score = sequence1.size() + sequence2.size() + count_zeros - score.size();

  // Similarity of the two sequences is the mean of the note similarity scores
  float score_1 = score_sum / ( count_score * 1.0 );

  // including length in score, if at least 5 notes
  if ( tf1.size() > 5 ) {
    score_1 += score_1 * seq_1_time_del / 500000.0; // 50 seconds yield 10% increase
  }

  if ( disp ) {
    // TODO: Draw graph
  }

  // TODO: Figure out what these are
  float mean_offset1 = 0;
  float mean_offset2 = 0;

  return make_tuple( lastmatch1, lastmatch2, mean_offset1, mean_offset2, score_1 );
}

tuple<int, int, float, float, float> two_way_similarity( vector<midi_event> tf1,
                                                         vector<midi_event> tf2,
                                                         bool disp = false )
{
  tuple<int, int, float, float, float> score1_data = musical_similarity( tf1, tf2, disp );
  tuple<int, int, float, float, float> score2_data = musical_similarity( tf2, tf1, disp );

  if ( get<4>( score1_data ) > get<4>( score2_data ) ) {
    return score1_data;
  }
  return score2_data;
}

vector<match> calculate_similarity_time( vector<midi_event> notes,
                                         vector<int> source_id,
                                         int currTime,
                                         bool disp = false,
                                         int skip = 100 )
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
  bool rewinded = false;
  int target_id_end = 0;
  int target_id_start = 0;
  while ( target_start < currTime - 5000 ) {
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
      if ( rewinded ) {
        rewinded = false;
      } else if ( lm1 >= source_id_start - source_id_end - 2 ) {
        target_time = notes[target_id_end + lm2].timestamp + ( currTime - notes[source_id_end + lm1].timestamp );
      } else if ( lm2 >= target_id_start - target_id_end - 2 ) {
        if ( currTime - notes[source_id_end + lm1].timestamp < 0 ) {
          cout << "oops something went wrong with time calculations - might end in infinite loop" << endl;
        }
        target_start += currTime - notes[source_id_end + lm1].timestamp;
        continue;
      }

      if ( target_time < currTime - 5000 ) {
        matches.emplace_back(
          currTime, target_time, score, source_id_start, source_id_end, target_id_end + lm2, target_id_end, 0, 0 );
      }
    }
    last_id_end = target_id_end;
    last_id_start = target_id_start;
    target_start += skip;
  }
  return matches;
}

vector<int> get_source_notes( vector<midi_event> notes, int start_time, int min_time )
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

void program_body( const string& midiPath, const string& outputPath )
{
  vector<midi_event> events = midi_to_timeseries( midiPath );
  vector<int> sims_arr;

  ofstream outputFile;
  outputFile.open(outputPath);
  outputFile << "source_timestamp,target_timestamp,score\n";
  
  for ( int i = START; i < END; i += SKIP ) {
    vector<match> matches = find_matches_at_timestamp( i, events, false );

    for ( match m : matches ) {
      outputFile << to_string(m.currTime) + "," + to_string(m.target_time) + "," + to_string(m.score) + "\n";
    }
  }
  outputFile.close();

}

void usage_message( const string_view argv0 )
{
  cerr << "Usage: " << argv0 << " [MIDI file] [Match Output CSV Name]\n";
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 3 ) {
      usage_message( argv[0] );
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2] );

  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}