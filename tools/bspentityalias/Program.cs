using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using System.Linq;

record struct LinePos(int Row, int Column);
record struct StreamToken(string Token, LinePos Start, LinePos End)
{
	public override string ToString() => Token.Length == 0 ? "EOF" : string.Concat('"', Token, '"');
}

class StreamTokenizer
{
	TextReader Reader;
	LinePos CurrentPos;

	public StreamTokenizer(TextReader reader)
	{
		Reader = reader;
	}

	int ReadChar()
	{
		int c = Reader.Read();

		// eof, so just return
		if (c == -1 || c == 0)
			return c;
		// new line, increase row, reset col
		else if (c == '\n')
		{
			CurrentPos.Row++;
			CurrentPos.Column = 0;
		}
		// otherwise, increase col
		else
			CurrentPos.Column++;

		return c;
	}

	public StreamToken Token()
	{
		string s = "";
		int c;

		// skip whitespace
		skipwhite:
		while (Reader.Peek() <= ' ')
		{
			c = ReadChar();
			if (c == 0 || c == -1)
				return new StreamToken("", CurrentPos, CurrentPos);
		}

		LinePos start = CurrentPos;

		c = ReadChar();

		// skip // comments
		if (c == '/' && Reader.Peek() == '/')
		{
			ReadChar();
			do
			{
				c = ReadChar();
			} while (c != '\n' && c != -1);

			goto skipwhite;
		}

		// handle quoted strings specially
		if (c == '\"')
		{
			while (true)
			{
				c = ReadChar();

				if (c == '\"' || c == 0 || c == -1)
					return new StreamToken(s, start, CurrentPos);

				s += (char) c;
			}
		}

		// parse a regular word
		LinePos end;
		do
		{
			end = CurrentPos;
			s += (char) c;
		} while ((c = ReadChar()) > 32);

		return new StreamToken(s, start, end);
	}

#if DEBUG
	public static void CheckTokens(TextReader reader, params StreamToken[] tokens)
	{
		StreamTokenizer tokenizer = new StreamTokenizer(reader);

		foreach (var token in tokens)
		{
			var check = tokenizer.Token();
			Debug.Assert(check == token);
		}
	}
#endif
}

static class Application
{
	static void PrintUsage()
	{
		Console.WriteLine("Entity Alias Substituter");
		Console.WriteLine("usage: " + AppDomain.CurrentDomain.FriendlyName + " [aliases.def] path-to-map.bsp");
		Console.WriteLine("outputs path-to-map.bsp.ent with the replaced aliased files, to be merged back into the BSP.");
	}

	static bool IsIdentifier(StreamToken s) => s.Token.Length > 0 && s.Token.All(c => char.IsLetterOrDigit(c) || c == '_');

	static void Expect(string file, StreamToken token, Func<StreamToken, bool> validator, string error)
	{
		if (validator(token))
			return;

		Console.WriteLine("{0}({1},{2}): " + error, file, token.Start.Row, token.Start.Column, token);
		Console.WriteLine("Press any key to exit...");
		Console.ReadKey(true);
		Environment.Exit(-1);
	}

	static void Main(string[] args)
	{
#if DEBUG
		StreamTokenizer.CheckTokens(new StringReader("hey there tokens!"),
			new StreamToken("hey", new LinePos(0, 0), new LinePos(0, 3)),
			new StreamToken("there", new LinePos(0, 4), new LinePos(0, 9)),
			new StreamToken("tokens!", new LinePos(0, 10), new LinePos(0, 17)),
			new StreamToken("", new LinePos(0, 17), new LinePos(0, 17)));
		StreamTokenizer.CheckTokens(new StringReader("// comment\r\nnew tokens here\r\n\ntoken\n// comment\n// multi line"),
			new StreamToken("new", new LinePos(1, 0), new LinePos(1, 3)),
			new StreamToken("tokens", new LinePos(1, 4), new LinePos(1, 10)),
			new StreamToken("here", new LinePos(1, 11), new LinePos(1, 15)),
			new StreamToken("token",  new LinePos(3, 0), new LinePos(3, 5)),
			new StreamToken("", new LinePos(5, 13), new LinePos(5, 13)));
		StreamTokenizer.CheckTokens(new StringReader("yo \"quoted string!!!\" hehehe"),
			new StreamToken("yo", new LinePos(0, 0), new LinePos(0, 2)),
			new StreamToken("quoted string!!!", new LinePos(0, 3), new LinePos(0, 21)),
			new StreamToken("hehehe", new LinePos(0, 22), new LinePos(0, 28)),
			new StreamToken("", new LinePos(0, 28), new LinePos(0, 28)));
#endif

		if (args.Length == 0 || args.Length > 2)
		{
			PrintUsage();
			return;
		}

		var bspFile = new FileInfo(args.Length == 2 ? args[1] : args[0]);

		if (!bspFile.Exists)
		{
			PrintUsage();
			Console.WriteLine("File \"{0}\" does not exist", args.Length == 2 ? args[1] : args[0]);
			return;
		}

		var aliasFile = (args.Length == 2) ? args[0] : "aliases.def";
		var file = new FileInfo(aliasFile);

		if (!file.Exists)
		{
			file = new FileInfo(Path.Join(bspFile.DirectoryName, aliasFile));

			if (!file.Exists)
			{
				PrintUsage();
				Console.WriteLine("Alias definition file \"{0}\" does not exist (checked relative to current dir & relative to path-to-map.bsp's directory)", aliasFile);
				return;
			}
		}

		var tokenizer = new StreamTokenizer(file.OpenText());
		var entityAliases = new Dictionary<string, Dictionary<string, string>>();

		while (true)
		{
			var entityNameToken = tokenizer.Token();

			if (entityNameToken.Token.Length == 0)
				break;

			Expect(aliasFile, entityNameToken, IsIdentifier, "expected an identifier (a-z 0-9 _), found {3}");

			var entityName = entityNameToken.Token.ToLower();

			Expect(aliasFile, entityNameToken with { Token = entityName }, s => !entityAliases.ContainsKey(s.Token), "alias already found earlier in file");

			var dict = (entityAliases[entityName] = new());

			Expect(aliasFile, tokenizer.Token(), s => s.Token == "{", "expected {{, found {3}");

			while (true)
			{
				var entityKeyToken = tokenizer.Token();

				Expect(aliasFile, entityKeyToken, s => IsIdentifier(s) || s.Token == "}", "expected an identifier (a-z 0-9 _) or }}, found {3}");

				if (entityKeyToken.Token == "}")
					break;

				var entityKey = entityKeyToken.Token.ToLower();

				Expect(aliasFile, entityKeyToken with { Token = entityKey }, IsIdentifier, "expected an identifier (a-z 0-9 _), found {3}");

				Expect(aliasFile, entityKeyToken, s => !dict.ContainsKey(s.Token), "key already provided earlier in alias");

				var entityValueToken = tokenizer.Token();

				Expect(aliasFile, entityValueToken, s => s.Start != s.End, "empty value");

				dict[entityKey] = entityValueToken.Token;
			}
		}

		Console.WriteLine("Loaded {0} alias definitions.", entityAliases.Count);

		// find the entity lump
		var bspBytes = File.ReadAllBytes(bspFile.FullName);
		long bytePosition = 0;
		long entityLumpPosition = -1, entityLumpSize = -1;
		string detectionString = "\"classname\"";

		while (true)
		{
			restart:
			if (bytePosition == bspBytes.LongLength)
				break;

			foreach (var c in detectionString)
				if (bspBytes[bytePosition++] != c)
					goto restart;

			for (entityLumpPosition = bytePosition; entityLumpPosition >= 0; --entityLumpPosition)
				if (bspBytes[entityLumpPosition] == '{')
					break;

			break;
		}

		if (entityLumpPosition == 0)
		{
			Console.WriteLine("Can't find start of entity lump in {0}", bspFile);
			Environment.Exit(-1);
		}

		for (var i = entityLumpPosition; ; i++)
		{
			entityLumpSize++;

			if (i >= bspBytes.LongLength)
			{
				Console.WriteLine("Can't find end of entity lump in {0}", bspFile);
				Environment.Exit(-1);
			}

			if (bspBytes[i] == 0)
				break;
		}

		// pull out the entities in the map
		var entityTokenizer = new StreamTokenizer(new StreamReader(new MemoryStream(bspBytes, (int)entityLumpPosition, (int)entityLumpSize, false)));
		var entities = new List<Dictionary<string, string>>();

		while (true)
		{
			var braceToken = entityTokenizer.Token();

			if (braceToken.Token.Length == 0)
				break;

			Expect(aliasFile, braceToken, s => s.Token == "{", "expected {{, found {3}");

			var ent = new Dictionary<string, string>();

			while (true)
			{
				var entityKeyToken = entityTokenizer.Token();

				Expect(aliasFile, entityKeyToken, s => IsIdentifier(s) || s.Token == "}", "expected an identifier (a-z 0-9 _) or }}, found {3}");

				if (entityKeyToken.Token == "}")
					break;

				var entityKey = entityKeyToken.Token.ToLower();

				Expect(aliasFile, entityKeyToken with { Token = entityKey }, IsIdentifier, "expected an identifier (a-z 0-9 _), found {3}");

				Expect(aliasFile, entityKeyToken, s => !ent.ContainsKey(s.Token), "key already provided earlier in alias");

				var entityValueToken = entityTokenizer.Token();

				Expect(aliasFile, entityValueToken, s => s.Start != s.End, "empty value");

				ent[entityKey] = entityValueToken.Token;
			}

			entities.Add(ent);
		}

		foreach (var entity in entities)
		{
			if (!entity.ContainsKey("classname"))
				continue;

			if (!entityAliases.ContainsKey(entity["classname"]))
				continue;

			var key = entity["classname"];
			var alias = entityAliases[key];

			foreach (var pair in alias)
			{
				if (pair.Key == "classname" || !entity.ContainsKey(pair.Key))
					entity[pair.Key] = pair.Value;
			}

			Console.WriteLine("aliasing {0} -> {1}...", key, entity["classname"]);
		}

		Console.WriteLine("done; writing output...");

		var outFile = new FileInfo(Path.Join(bspFile.DirectoryName, bspFile.Name + ".ent"));
		using (var outStream = outFile.CreateText())
		{
			foreach (var entity in entities)
			{
				outStream.WriteLine("{");

				foreach (var pair in entity)
					outStream.WriteLine("\t\"{0}\" \"{1}\"", pair.Key, pair.Value);

				outStream.WriteLine("}");
			}
		}

		Console.WriteLine("finished");
	}
}