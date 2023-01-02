# WoodokuAI
![A sped-up video of WoodokuAI running](https://i.imgur.com/h7qSzxg.gif)\
An AI that plays Woodoku by itself. (Or with a phone)

## The game
There is a considerable amount of sudoku-meets-tetris type of block-filling games on the internet and on mobile app stores, and I am not entirely sure which is the "original".\
[Woodoku by Tripledot Studios](https://play.google.com/store/apps/details?id=com.tripledot.woodoku) is one of them, which this code implements and plays. Between countless clones and lookalikes, such as [1010!](https://play.google.com/store/apps/details?id=com.gramgames.tenten), [Blockudoku](https://play.google.com/store/apps/details?id=com.easybrain.block.puzzle.games), [and](https://play.google.com/store/apps/details?id=puzzle.blockpuzzle.cube.relax) [more](https://play.google.com/store/apps/details?id=wood.blockpuzzle.game.jewel.classic) the rules are slightly different - such as 3x3 squares not counting as a clear, different piece sets, different board sizes, and different scoring criteria.

## Building
There are no special dependencies, so any linux system with a basic build environment would work fine. Run `make` to create the `WoodokuAI` executable. Haven't tested on any other OSes.

## Running
Run the `WoodokuAI` executable to watch it play the game. Once you run it, the AI will start playing the game by itself, until it runs out of valid moves.

By default, the executable will run by itself, with 4 threads at 3 seconds per turn. You can use command-line arguments to tweak the performance or the game itself.
Run `WoodokuAI --help` to list all the command-line options.

Example:\
`./WoodokuAI --thread 16 --millisec-per-turn 1000 --randsearch-min 20 --disable-board-fitness`

## Optimality
The performance of the AI is not so great. In Woodoku, you only get 3 pieces at a time, with future pieces being a complete mystery. This creates a considerable amount of uncertainty in the game, which makes the game nearly impossible to solve.
I found that most games end at around 100 turns or less, no matter how much time I give the AI. The optimal play differs so much depending on which piece comes next, which makes the entire search futile.

However, you can eliminate this uncertainty by supplying the `--deterministic` option, which will reveal all pieces ahead of time. This makes the game incredibly easy to the AI, and the AI can easily survive tens of thousands of turns in this mode.

## Python Android Driver
There is a python script that can play the actual Woodoku application running on a physical Android phone. The Python android driver and the C++ game AI communicates through a socket connection. The Python driver acts as a server, and game AI acts as a client.

Running the Python script requires `ADB` to be installed in your system. And `pillow` package in your python installation.

The script has piece locations and grid coordinates as magic numbers in the code itself, so if your phone has a different screen resolution than 1080x2400, the program will probably break. All the magic numbers are in the top of the script, so you can try tweaking it yourself.

To auto-play the game on Android, run
`python3 woodoku_game_server.py`
and, on another terminal, run
`./WoodokuAI --server-game`
and both programs should work together and (try to) automatically play the game on your phone.


