import os
example_path = "tutorial"
out_path = "web/tutorial.html"
example_entries = os.listdir(example_path)
with open(out_path, 'w') as f:
    f.write("""
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
    """)

    f.write("<body>")
    for tutorial_path in example_entries:
        pretty_name = tutorial_path.replace("_", " ")[:-4] # get rid of `.oct`
        f.write("<h2>{}</h2>".format(pretty_name))
        f.write("<pre><code>")
        with open(example_path + "/" + tutorial_path, 'r') as tutorial_f:
            f.write(tutorial_f.read())
        f.write("</code></pre>")
    f.write("</body>")
    f.write("</html>")
