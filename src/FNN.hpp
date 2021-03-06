/*
DigitScanner - Copyright (C) 2016 - Olivier Deiss - olivier.deiss@gmail.com

DigitScanner is a C++ tool to create, train and test feedforward neural
networks (fnn) for handwritten number recognition. The project uses the
MNIST dataset to train and test the neural networks. It is also possible
to draw numbers in a window and ask the tool to guess the number you drew.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
This class defines a feedforward neural network (fnn) and the associated
methods for initializing, training, and computation of output value.
This class is a template to allow the use of multiple types for the data
stored in the neural network (basically allows float, double, long double).

A neural network is composed of multiple layers. These layers are defined
with the abstract class FNNLayer. The input layer is of type FNNInputLayer,
while the other layers and the output layer are of type FNNFullyConnectedLayer.

                     ------------
                     | FNNLayer |
                     ------------
                        ^   ^
                       /     \
                      /       \
                     /         \
       -----------------     --------------------------
       | FNNInputLayer |     | FNNFullyConnectedLayer |
       -----------------     --------------------------
        
An FNNInputLayer just has a number of nodes, while a FNNFullyConnectedLayer
has weight and bias matrices. The main purpose of this scheme is to be able
to pass a pointer to the previous layer when creating a new one, might it
be an input layer or a fully connected layer.
*/

#ifndef FNN_hpp
#define FNN_hpp

#include <cmath>
#include <list>
#include <iostream>
#include <fstream>
#include <map>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#include "Matrix.hpp"

template<typename T> class FNNInputLayer;
template<typename T> class FNNFullyConnectedLayer;

template<typename T>
class FNN {

    typedef std::chrono::time_point<std::chrono::high_resolution_clock> chrono_clock;
    typedef std::pair<std::vector<Matrix<T>>, std::vector<Matrix<T>>>   nabla_pair;

    public:

        FNN(std::vector<int>);
        ~FNN();
    
        int                        get_nb_fully_connected_layers()  const { return nb_fully_connected_layers; }
        std::vector<int>           get_layers()                     const { return layers; }
        FNNFullyConnectedLayer<T>* get_fully_connected_layer(int i) const { return fully_connected_layers[i]; }
    
        const Matrix<T>        feedforward(Matrix<T>*);
        std::vector<Matrix<T>> feedforward_complete(Matrix<T>*);
        void                   random_init_values(FNNFullyConnectedLayer<T>*);
        void                   SGD_batch(std::vector<Matrix<T>>, std::vector<Matrix<T>>, const int, const int, const double, const double);
    
    private:
    
        double     elapsed_time(chrono_clock);
        nabla_pair backpropagation_cross_entropy(Matrix<T>&, Matrix<T>&);
    
        std::vector<int>            layers;
        FNNInputLayer<T>*           input;
        int                         nb_fully_connected_layers;
        FNNFullyConnectedLayer<T>** fully_connected_layers;
    
};

template<typename T>
class FNNLayer {

    public:
    
        FNNLayer(int nb_nodes) :
            nb_nodes(nb_nodes) {}
virtual ~FNNLayer() {}
        int get_nb_nodes() { return nb_nodes; }
    
    protected:
    
        int nb_nodes;
    
};

template<typename T>
class FNNInputLayer: public FNNLayer<T> {

    public:
    
        FNNInputLayer(int nb_nodes) :
            FNNLayer<T>(nb_nodes) {}
virtual ~FNNInputLayer() {}

};

template<typename T>
class FNNFullyConnectedLayer: public FNNLayer<T> {

    public:
    
        FNNFullyConnectedLayer(int nb_nodes, FNNLayer<T>* p_previous_layer) :
            FNNLayer<T>(nb_nodes),
            previous_layer(p_previous_layer),
            W(nb_nodes, previous_layer->get_nb_nodes()),
            B(nb_nodes, 1) {}
virtual ~FNNFullyConnectedLayer() {}
    
        FNNLayer<T>* get_previous_layer() { return previous_layer; }
        Matrix<T>*   get_biases()         { return &B; }
        Matrix<T>*   get_weights()        { return &W; }
    
    private:
    
        FNNLayer<T>* previous_layer;
        Matrix<T>    W;
        Matrix<T>    B;
    
};



/*
Initializes the variables and creates the layers according to the
p_layer vector. The layers are linked to each other.
*/
template<typename T>
FNN<T>::FNN(std::vector<int> p_layers) :
    layers(p_layers),
    input(new FNNInputLayer<T>(p_layers[0])),
    nb_fully_connected_layers(static_cast<int>(p_layers.size())-1),
    fully_connected_layers(new FNNFullyConnectedLayer<T>*[nb_fully_connected_layers]) {
    FNNLayer<T>* previous = input;
    for(int i=0 ; i<nb_fully_connected_layers ; i++) {
        FNNFullyConnectedLayer<T>* l = new FNNFullyConnectedLayer<T>(layers[i+1], previous);
        fully_connected_layers[i]    = l;
        previous                     = l;
        random_init_values(l);
    }
}

/*
Deletes the input, hidden and output layers.
*/
template<typename T>
FNN<T>::~FNN() {
    delete input;
    for(int i=0 ; i<nb_fully_connected_layers ; i++) delete fully_connected_layers[i];
    delete [] fully_connected_layers;
}

/*
Backpropagation algorithm using the cross-entropy cost function. The algorithm
computes the difference between the output for a given set of input and the 
expected output. This gives what should be corrected. This first step is the
feedforward step. The next step is the backpropagation, in which all the weights
and biases differences are computed. This function returns these differences
for one pair of input and output. Usually, the training is done using batches
of input-output data. The parameters are updated only once for the whole batch
of data. Below is the whole mathematical explanation.

The goal of this algorithm is to reduce the cross-entropy cost C defined as

        C = - [ y*ln(a) + (1-y)*ln(1-a) ]
        a = sigmoid(w*a_ + b)                 
    
With a_ the activations of the previous layer. The cross-entropy has the advantage
of enabling the network to learn faster when it is further from the truth, which
is not achievable with the common quadratic cost function 1/2 * ||y-a||^2.

The gradient descent algorithm is an iterative algorithm. At each step, C is
updated by substracting DeltaC:

        DeltaC = NablaC * DeltaX

With Nabl a being the derivative.To make sure DeltaC is always positive so
the cost is effectively reduced at every step, it is possible to choose:

        DeltaX = - n * NablaC
        DeltaC = - n * ||NablaC||^2
        
With n the learning rate. What we will do here is:

        w --> w - n * NablaCw
        b --> b - n * NablaCb
 
So we need to compute NablaCW and NablaCB:

        d = (a-y)
        NablaCw = dC/dw = d * a_
        NablaCb = dC/db = d
        
The calculus involves sigmoid'(z) = sigmoid(z)*(1-sigmoid(z)), since
sigmoid(z) = 1/(1+e^(−z)).

For the previous layers (x_ means x for the previous layer):

        NablaCw_ = dC/da * da/dz * dz/da_ * da_/dw_
        NablaCb_ = dC/da * da/dz * dz/da_ * da_/db_
        
The calculus gives:

        dC/da * da/dz = (a-y)       // same as first step
        dz/da_ = w                  // a = sig(z)    and  z  = w*a_ + b
        da_/dw_ = a_*(1-a_) * a__   // a_ = sig(z_)  and  z_ = w_*a__ + b_
        da_/db_ = a_*(1-a_)
        
So:

        NablaCw_ = (a-y) * w * a_*(1-a_) * a__ = d * w * a_*(1-a_) * a__
        NablaCb_ = (a-y) * w * a_*(1-a_)       = d * w * a_*(1-a_)

This is what is computed by this function, using matrices. Suppose we have
the following 4 layers FNN with 2 hidden layers:

        O
                O       O
        O                       O
                O       O
        O                       O
                O       O
        O
 
        A1      A2      A3      A4        activations for the given layers
            W1      W2      W3            weight matrices between the layers
            B1      B2      B3            bias matrices between the layers

We first compute the nabla for matrices W and B, using the formula:

        D(3)   = A(4) - Y
        NCW(3) = D(3) * A(3)^t
        NCB(3) = D(3)

This is how W3 and B3 needs to be updated. For the previous W and B matrices, 
matrix D is propagated from layer to layer as follow:

        SP   = [ (1) - A(k+1) ] ° A(k+1)   // stands for sigmoid'
        D(k) = [ W(k)^t * D(k+1) ] ° S
        
And then used to compute NCW and NCB:

        NCW(k) = D(k) * A(k)^t
        NCB(k) = D(k)

In these expressions:
        X^t means transpose of X
        (1) means a column of ones of height that of A(k+1).
         °  means an element wise product (Hadamard product)
         *  means a product of matrices
*/
template<typename T>
typename FNN<T>::nabla_pair FNN<T>::backpropagation_cross_entropy(Matrix<T>& training_input, Matrix<T>& training_output) {
    /* feedforward */
    std::vector<Matrix<T>> activations = feedforward_complete(&training_input);
    /* backpropagation */
    std::vector<Matrix<T>> nabla_CW; nabla_CW.resize(nb_fully_connected_layers);
    std::vector<Matrix<T>> nabla_CB; nabla_CB.resize(nb_fully_connected_layers);
    Matrix<T> D(activations[nb_fully_connected_layers], true);
    Matrix<T> At(activations[nb_fully_connected_layers-1], true);
        D -= training_output;
        At.self_transpose();
    Matrix<T> NCW(D, true);
        NCW *= At;
        At.free();
    nabla_CW[nb_fully_connected_layers-1] = NCW;
    nabla_CB[nb_fully_connected_layers-1] = D;
    activations[nb_fully_connected_layers].free();
    /* activations[0] = input, do not free */
    /* backward propagation */
    for(int i=nb_fully_connected_layers-2 ; i>=0 ; i--) {
        Matrix<T> Wt(fully_connected_layers[i+1]->get_weights(), true);
            Wt.self_transpose();
            D = Wt*D;
            Wt.free();
        Matrix<T>* A = &activations[i+1];
        Matrix<T> SP(A->get_I(), 1);
            SP.fill(1);
            SP -= A;
            SP.element_wise_product(A);
            D.element_wise_product(SP);
            SP.free();
        Matrix<T> At(activations[i], true);
            At.self_transpose();
        Matrix<T> NCW(D, true);
            NCW *= At;
            At.free();
        nabla_CW[i] = NCW;
        nabla_CB[i] = D;
        activations[i+1].free();
    }
    return nabla_pair(nabla_CW, nabla_CB);
}

/*
Feedforward algorithm to be used to compute the output.
O = WA+B. This function uses the sigmoid function to range
the output in [0 1]. This function is to be called when just
the output is needed.
*/
template<typename T>
const Matrix<T> FNN<T>::feedforward(Matrix<T>* X) {
    std::vector<Matrix<T>> activations;
    activations.push_back(*X);
    for(int i=0 ; i<nb_fully_connected_layers ; i++) {
        FNNFullyConnectedLayer<T>* layer = fully_connected_layers[i];
        Matrix<T> a(layer->get_weights(), true);
            a *= activations[i];
            a += layer->get_biases();
            a.sigmoid();
            activations.push_back(a);
            if(i>0) activations[i].free();
    }
    return activations[nb_fully_connected_layers];
}

/*
Feedforward algorithm to be used in the backpropagation algorithm.
This function is to be called when all the activations are needed,
for instance during the backpropagation step.
*/
template<typename T>
std::vector<Matrix<T>> FNN<T>::feedforward_complete(Matrix<T>* X) {
    std::vector<Matrix<T>> activations;
    activations.push_back(*X);
    for(int i=0 ; i<nb_fully_connected_layers ; i++) {
        FNNFullyConnectedLayer<T>* layer = fully_connected_layers[i];
        Matrix<T> a(layer->get_weights(), true);
            a *= activations[i];
            a += layer->get_biases();
            a.sigmoid();
            activations.push_back(a);
    }
    return activations;
}

/*
Initializes the network's weights and biases with a Gaussian generator.
*/
template<typename T>
void FNN<T>::random_init_values(FNNFullyConnectedLayer<T>* l) {
    Matrix<T> W = l->get_weights();
    Matrix<T> B = l->get_biases();
    std::default_random_engine       generator;
    std::normal_distribution<double> gauss_biases(0, 1);
    std::normal_distribution<double> gauss_weights(0, 1.0/sqrt(l->get_previous_layer()->get_nb_nodes()));
    for(int i = 0 ; i<W.get_I() ; i++) {
        for(int j = 0 ; j<W.get_J() ; j++) W(i, j) = gauss_weights(generator);
        B(i, 0) = gauss_biases(generator);
    }
}

/*
Stochastic Gradient Descent algorithm for a batch.
This function is the actual SGD algorithm. It runs the backpropagation
on the whole batch before updating the weights and biases.
*/
template<typename T>
void FNN<T>::SGD_batch(std::vector<Matrix<T>> batch_input, std::vector<Matrix<T>> batch_output, const int training_set_len, const int batch_len, const double eta, const double alpha) {
    /* create nabla matrices vectors */
    std::vector<Matrix<T>> nabla_CW;
    std::vector<Matrix<T>> nabla_CB;
    for(int i=0 ; i<nb_fully_connected_layers ; i++) {
        nabla_CW.emplace_back(layers[i+1], layers[i]); nabla_CW.back().fill(0);
        nabla_CB.emplace_back(layers[i+1], 1);         nabla_CB.back().fill(0);
    }
    /* feedforward-backpropagation for each data in the batch and sum the nablas */
    for(int i=0 ; i<batch_len ; i++) {
        nabla_pair delta_nabla = backpropagation_cross_entropy(batch_input[i], batch_output[i]);
        for(int j=0 ; j<nb_fully_connected_layers ; j++) {
            nabla_CW[j] += delta_nabla.first[j];  delta_nabla.first[j].free();
            nabla_CB[j] += delta_nabla.second[j]; delta_nabla.second[j].free();
        }
    }
    /* update the parameters */
    for(int i=0 ; i<nb_fully_connected_layers ; i++) {
        nabla_CW[i] *= eta/static_cast<double>(batch_len);
        nabla_CB[i] *= eta/static_cast<double>(batch_len);
        fully_connected_layers[i]->get_weights()->operator*=((1-(alpha*eta)/static_cast<double>(training_set_len)));
        fully_connected_layers[i]->get_weights()->operator-=(&nabla_CW[i]);
        fully_connected_layers[i]->get_biases()->operator-=(&nabla_CB[i]);
        nabla_CW[i].free();
        nabla_CB[i].free();
    }
}

/*
Computes execution time.
*/
template<typename T>
double FNN<T>::elapsed_time(chrono_clock begin) {
    chrono_clock end = std::chrono::high_resolution_clock::now();
    auto         dur = end - begin;
    auto         ms  = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    return static_cast<int>(ms/10.0)/100.0;
}

#endif
