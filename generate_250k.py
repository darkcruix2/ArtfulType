text = "This is a very long file to test the 250K limits of the ArtfulType 0.20 pro text editor.\n"
with open("test_250k.md", "w") as f:
    f.write(text * 3000)
