#pragma once
#include "backprop.hh"
#include "cdf.hh"
#include "gradient_descent.hh"
#include "html.hh"
#include "inference.hh"
#include "network.hh"
#include "nngraph.hh"
#include "randomize_network.hh"
#include "web_server.hh"

#include <optional>
#include <thread>

template<NetworkT Network, size_t batch_size>
class TrainingVisualizer
{
public:
  using Infer = NetworkInference<Network, batch_size>;
  using Input = typename Infer::Input;
  using Output = typename Infer::Output;
  using BackProp = NetworkBackPropagation<Network, batch_size>;
  using GradientDescent = NetworkGradientDescent<Network, batch_size>;
  using LossFunction = std::function<float( Output, Output )>;
  using GradientOfLossFunction = std::function<Output( Output, Output )>;
  using FinalLayerActivationFunction = std::optional<std::function<float( float )>>;
  using Graph = NetworkGraph<Network>;

private:
  EventLoop events_ {};
  WebServer web_server_;
  float learning_rate_;
  size_t iteration_ = 0;
  std::thread server_thread_;
  bool should_exit_ = false;
  LossFunction loss_fn_;
  GradientOfLossFunction pd_loss_fn_wrt_outputs_;
  FinalLayerActivationFunction final_layer_activation_fn_;
  RandomState rng_ {};

  std::mutex network_mutex_ {};
  std::unique_ptr<Network> nn_ = std::make_unique<Network>();
  std::unique_ptr<Network> old_nn_ = std::make_unique<Network>();
  std::unique_ptr<Infer> infer_ = std::make_unique<Infer>();
  std::unique_ptr<BackProp> backprop_ = std::make_unique<BackProp>();
  std::unique_ptr<GradientDescent> gradient_descent_ = std::make_unique<GradientDescent>();
  std::unique_ptr<Input> input_ = std::make_unique<Input>();
  std::unique_ptr<Output> expected_ = std::make_unique<Output>();
  std::unique_ptr<Output> pd_loss_wrt_outputs_ = std::make_unique<Output>();

  std::unique_ptr<Graph> graph = std::make_unique<NetworkGraph<Network>>();

  using Column = std::vector<double>;
  using Weights = std::vector<Column>;
  using Biases = std::vector<Column>;
  using Activations = std::vector<Column>;

public:
  TrainingVisualizer( const LossFunction& loss_fn,
                      const GradientOfLossFunction& pd_loss_fn_wrt_outputs,
                      const FinalLayerActivationFunction final_layer_activation_fn = std::nullopt,
                      float default_learning_rate = 0.01 )
    : web_server_( events_,
                   12345,
                   [&]( const HTTPRequest& request, HTTPResponse& response ) { handler( request, response ); } )
    , learning_rate_( default_learning_rate )
    , server_thread_( [&]() { start(); } )
    , loss_fn_( loss_fn )
    , pd_loss_fn_wrt_outputs_( pd_loss_fn_wrt_outputs )
    , final_layer_activation_fn_( final_layer_activation_fn )
  {
    randomize();
  }

  void train( const Input& input, const Output& output )
  {
    using namespace std;
    lock_guard<mutex> lock( network_mutex_ );

    *input_ = input;
    *expected_ = output;

    infer_->apply( *nn_, *input_ );

    *pd_loss_wrt_outputs_ = pd_loss_fn_wrt_outputs_( *expected_, infer_->output() );

    backprop_->differentiate( *nn_, *input_, *infer_, *pd_loss_wrt_outputs_ );

    old_nn_ = make_unique<Network>( *nn_ );

    gradient_descent_->update( *nn_, *backprop_, learning_rate_ );
    iteration_++;
  }

  void randomize()
  {
    using namespace std;
    lock_guard<mutex> lock( network_mutex_ );
    randomize_network( *nn_, rng_ );
    old_nn_ = make_unique<Network>( *nn_ );
    iteration_ = 0;
  }

  ~TrainingVisualizer()
  {
    should_exit_ = true;
    server_thread_.join();
  }

private:
  void start()
  {
    std::cerr << "Starting server" << std::endl;
    while ( not should_exit_ and events_.wait_next_event( -1 ) != EventLoop::Result::Exit ) {}
    std::cerr << "Stopping server" << std::endl;
  }

  void handler( const HTTPRequest& request, HTTPResponse& response )
  {
    using namespace std;
    response.http_version = "HTTP/1.1";
    if ( request.request_target == "/" ) {
      return root_handler( response );
    }
    if ( request.request_target == "/input-output.svg" ) {
      return graph_handler( response );
    }
    if ( request.request_target == "/reset" ) {
      randomize();
      return success_handler( response );
    }
    if ( request.request_target == "/iteration" ) {
      return to_string_handler( response, iteration_ );
    }
    return not_found_handler( response );
  }

  void not_found_handler( HTTPResponse& response )
  {
    response.status_code = "404";
    response.reason_phrase = "Not found";
    response.headers.content_length = 0;
  }

  void success_handler( HTTPResponse& response )
  {
    response.status_code = "204";
    response.reason_phrase = "No content";
    response.headers.content_length = 0;
  }

  void redirect_handler( HTTPResponse& response )
  {
    response.status_code = "303";
    response.reason_phrase = "See other";
    response.headers.location = "/";
    response.headers.content_length = 0;
  }

  void string_handler( HTTPResponse& response, const std::string& s )
  {
    response.status_code = "200";
    response.reason_phrase = "OK";
    response.body = s;
    response.headers.content_type = "text/plain";
    response.headers.content_length = s.length();
  }

  template<class T>
  void to_string_handler( HTTPResponse& response, T x )
  {
    std::stringstream ss;
    ss << x;
    string_handler( response, ss.str() );
  }

  void root_handler( HTTPResponse& response )
  {
    using namespace std;
    lock_guard<mutex> lock( network_mutex_ );

    static const std::string js = R"js(
        async function load_graph() {
          image.innerHTML = await fetch('/input-output.svg').then(response => response.text());
        };
        async function load_iteration() {
          iteration.innerHTML = await fetch('/iteration').then(response => response.text());
        };

        window.onload = function(e) {
          setInterval(load_graph, 100)
          setInterval(load_iteration, 100)
        };
        )js";

    HTMLPage page( {
      HTMLElement::head( {} ),
      HTMLElement::body( {
        HTMLElement::h1( {}, { HTMLElement::text( "Training Visualizer" ) } ),
        HTMLElement::p(
          {},
          {
            HTMLElement::text( "Iteration: " ),
            HTMLElement::span( { make_pair( "id", "iteration" ) }, { HTMLElement::to_string( iteration_ ) } ),
          } ),
        HTMLElement::form(
          {
            make_pair( "action", "/reset" ),
            make_pair( "method", "post" ),
          },
          { HTMLElement::input(
            {
              make_pair( "type", "submit" ),
              make_pair( "value", "Reset" ),
            },
            {} ) } ),
        HTMLElement::div( { make_pair( "id", "image" ) }, {} ),
        HTMLElement::script( {}, js ),
      } ),
    } );

    std::stringstream ss;
    ss << page;
    html_handler( response, ss.str() );
  }

  void html_handler( HTTPResponse& response, const std::string_view s )
  {
    response.status_code = "200";
    response.reason_phrase = "OK";
    response.body = s;
    response.headers.content_type = "text/html";
    response.headers.content_length = s.length();
  }

  void graph_handler( HTTPResponse& response )
  {
    using namespace std;
    response.status_code = "200";
    response.reason_phrase = "OK";
    response.headers.content_type = "image/svg+xml";

    {
      lock_guard<mutex> lock( network_mutex_ );
      graph->initialize( *nn_ );
      graph->add_activations( *infer_ );
      graph->add_gradients( *backprop_ );
    }

    vector<pair<float, float>> target_vs_inferred;

    for ( ssize_t i = 0; i < expected_->size(); i++ ) {
      const auto e = ( *expected_ )( i );
      auto p = ( infer_->output() )( i );
      if ( final_layer_activation_fn_ ) {
        p = ( *final_layer_activation_fn_ )( p );
      }
      target_vs_inferred.push_back( make_pair( e, p ) );
    }

    response.body.append( graph->io_graph( target_vs_inferred, "Target vs. Inferred", "output" ) );
    response.body.append( "<p>" );

    auto layer_graphs = graph->layer_graphs();
    for ( const auto& layer : layer_graphs ) {
      response.body.append( layer );
      response.body.append( "<p>" );
    }
    response.headers.content_length = response.body.size();
  }
};
