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

#include <iostream>

#include "Arguments.hpp"
#include "DigitScanner.hpp"
#include "Window.hpp"

typedef std::chrono::time_point<std::chrono::high_resolution_clock> chrono_clock;

void print_elapsed_time(chrono_clock);

int main(int argc, char **argv) {

    /* parse arguments */
    Arguments args(argc, argv);
    int err = args.parse_arguments();
    if(err<0) {
        if(err==-2) {
            args.print_help();
        }
        if(err==-4) {
            args.print_license();
        }
        return 0;
    }
    
    /* initializations */
    srand(static_cast<unsigned int>(time(NULL)));
    
    /* DigitScanner */
    DigitScanner<float> dgs(args.max_threads);
    if(args.is_set("layers"))     { dgs.set_layers(args.layers); }
    else if(args.is_set("fnnin")) { if(!dgs.load(args.fnnin)) return 0; }
    
    /* actions */
    chrono_clock begin = std::chrono::high_resolution_clock::now();
    if(args.is_set("train"))     { dgs.train(args.mnist, args.train_imgnb, args.train_imgskip, args.train_epochs, args.train_batch_len, args.train_eta, args.train_alpha); }
    else if(args.is_set("test")) { dgs.test(args.mnist, args.test_imgnb, args.test_imgskip); }
    if(args.is_set("time"))      { print_elapsed_time(begin); }

    /* save */
    if(args.is_set("fnnout")) { dgs.save(args.fnnout); }
    
    /* gui */
    if(args.is_set("gui")) {
        Window *w = new Window(280, 280);
        w->setDgs(&dgs);
        w->setSceneWidth(280);
        w->init();
        w->launch();
    }
    
    return 0;
    
}

/*
Computes and print execution time.
*/
void print_elapsed_time(chrono_clock begin) {
    chrono_clock end = std::chrono::high_resolution_clock::now();
    auto         dur = end - begin;
    auto         ms  = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    std::cout << static_cast<double>(ms)/1000 << " s" << std::endl;
}
