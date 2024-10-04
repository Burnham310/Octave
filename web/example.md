<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Music Sequences</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #f4f4f4;
            margin: 0;
            padding: 20px;
        }
        h2 {
            color: #333;
            border-bottom: 2px solid #ddd;
            padding-bottom: 10px;
        }
        pre {
            background-color: #fff;
            border: 1px solid #ddd;
            padding: 10px;
            border-radius: 5px;
            white-space: pre-wrap;
            word-wrap: break-word;
        }
        code {
            color: #007acc;
            font-weight: bold;
        }
    </style>
</head>
<body>

    <h2>Twinkle</h2>
    <pre><code>
main = |:: 1.. 1... 5.. 5... 6.. 6... 5... 4.. 4... 3.. 3... 2.. 2... 1..|
    </code></pre>

    <h2>Tchaikovsky</h2>
    <pre><code>
main = | :scale=/F 5 MIN/, bpm=90:

  1..  [1 3 5]..  6...  5.... 
  [3 5]..  4..  [4 6]...  3...

  1..  [1 3 5]..  6...  5....
  [2 4]..  3...  2...  1....

  [5 7]..  [4 6]...  5....  [3 5]...
  1..  [1 3 5]..  6...  5....

  1...  [1 4 6]..  4....  [2 5]...
  5..  4..  [3 5]...  2....

|
    </code></pre>

    <h2>ELM</h2>
    <pre><code>
// ELM
// Genra: Midwest Emo
main = | :scale=/D 3 MAJ/, bpm=140, instrument=27:   // section configuration
    1.. oo'2.. o'7.. o'5..      
    2... 3... 5... 6... o'2... o#'4... o'5..

    1.. oo'2.. o'7.. o'5.. 
    2... 3... 5... 6... o'2... o#'4... o'5... oo'2...

    1.. oo'2.. o'7.. o'5.. 
    2... 3... 5... 6... o'2... o#'4... o'5.. 

    1.. oo'3.. o'7. 
|
// END
    </code></pre>

</body>
</html>
