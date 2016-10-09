using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace VsProjectGenerator
{
	class Constants
	{
		public static readonly Guid ProjectTypeGuid = new Guid("8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942"); // GUID for C++

		public const string FormatVersion = "12.00";
		public const string VisualStudioVersion = "14.0.24720.0";
		public const string MinimumVisualStudioVersion = "10.0.40219.1";

		public const string ToolsVersion = "14.0";
		public const string FiltersToolsVersion = "4.0";
		public const string XmlNamespace = "http://schemas.microsoft.com/developer/msbuild/2003";
		public const string MakeFileProjKeyword = "MakeFileProj";
		public const string ConfigurationType = "Makefile";
		public const string PlatformToolset = "v140";
		public const string CharacterSet = "MultiByte";

		public const string WarningLevel = "Level3";

		public static readonly string[] DefaultDefines =
			{ "$(NMakePreprocessorDefinitions)" };
		public static readonly string[] DefaultIncludeDirectories =
			{ "$(NMakeIncludeSearchPath)" };
	}

	class Solution
	{
		public Solution()
		{
			Projects = new List<Project>();
			SolutionConfigurationPlatforms = new List<SolutionConfigurationPlatform>();
		}

		public string Name { get; set; }
		public string RootSourceDirectory { get; set; }
		public List<Project> Projects { get; set; }
		public List<SolutionConfigurationPlatform> SolutionConfigurationPlatforms;
	}

	class SolutionConfigurationPlatform
	{
		public string Configuration { get; set; }
		public string Platform { get; set; }

		public string ConfigurationPlatform
		{
			get
			{
				return Configuration + "|" + Platform;
			}
		}
	}

	class Project
	{
		public Project()
		{
			ProjectConfigurationPlatforms = new List<ProjectConfigurationPlatform>();

			IncludeDirectories = new List<string>();
			RawSourceFiles = new List<string>();
			SourceFiles = new List<string>();
			IncludeFiles = new List<string>();
			InlineFiles = new List<string>();
			TextFiles = new List<string>();
			NatvisFiles = new List<string>();

			Directories = new List<string>();
		}

		public string Name { get; set; }
		public Guid Guid { get; set; }

		public List<ProjectConfigurationPlatform> ProjectConfigurationPlatforms { get; set; }

		public List<string> IncludeDirectories { get; set; }
		public List<string> RawSourceFiles { get; set; }
		public List<string> SourceFiles { get; set; }
		public List<string> IncludeFiles { get; set; }
		public List<string> InlineFiles { get; set; }
		public List<string> TextFiles { get; set; }
		public List<string> NatvisFiles { get; set; }

		public List<string> Directories { get; set; }
	}

	class ProjectConfigurationPlatform
	{
		public ProjectConfigurationPlatform()
		{
			Defines = new List<string>();
			IncludeDirectories = new List<string>();
		}

		public string Configuration { get; set; }
		public string Platform { get; set; }
		public string BuildCommand { get; set; }
		public string CleanCommand { get; set; }
		public string RebuildCommand { get; set; }
		public string OutputDir { get; set; }
		public List<string> Defines { get; set; }
		public List<string> IncludeDirectories { get; set; }
		public bool Is64Bit { get; set; }
		public bool IsDebug { get; set; }

		public string ConfigurationPlatform
		{
			get
			{
				return Configuration + "|" + Platform;
			}
		}

		public string ConditionString
		{
			get
			{
				return "'$(Configuration)|$(Platform)'=='" + ConfigurationPlatform + "'";
			}
		}
	}

	class Program
	{
		static int Main(string[] args)
		{
			try
			{
				bool clean = false;
				string path = null;

				if (args.Length == 0)
				{
					throw new ArgumentException("Invalid argument count");
				}
				else
				{
					for (int index = 0; index < args.Length; index++)
					{
						if (index == args.Length - 1)
						{
							path = args[index];
						}
						else if (args[index].Equals("--clean"))
						{
							clean = true;
						}
						else
						{
							throw new ArgumentException("Unknown argument '" + args[index] + "'");
						}
					}
				}

				if (clean)
				{
					RemoveExistingProjects(path);
				}
				else
				{
					Solution solution = ReadConfigFiles(path);

					if (solution == null)
					{
						throw new Exception("Failed to read config files");
					}

					AddIndividualFilesToProjects(solution);

					WriteSolution(solution.Name + ".sln", solution);
					foreach (Project project in solution.Projects)
					{
						System.IO.Directory.CreateDirectory(project.Name);
						WriteProject(project.Name + "\\" + project.Name + ".vcxproj", solution, project);
					}
				}

				return 0;
			}
			catch (Exception e)
			{
				Console.WriteLine(e.Message);
				return 1;
			}
		}

		static void RemoveExistingProjects(string path)
		{
			string[] vsSolutionConfigFiles = System.IO.Directory.GetFiles(path, "*.vs_solution_config");
			string[] vsSolutionProjectFiles = System.IO.Directory.GetFiles(path, "*.vs_project_config");
			string[] solutionFiles = System.IO.Directory.GetFiles(path, "*.sln");
			string[] vcdbFiles = System.IO.Directory.GetFiles(path, "*.VC.db");
			string[] directories = System.IO.Directory.GetDirectories(path);
			string[][] filesToDelete = { vsSolutionConfigFiles, vsSolutionProjectFiles, solutionFiles, vcdbFiles };

			try
			{
				foreach (string[] files in filesToDelete)
				{
					foreach (string file in files)
					{
						System.IO.File.Delete(System.IO.Path.Combine(path, file));
					}
				}

				foreach (string directory in directories)
				{
					System.IO.Directory.Delete(System.IO.Path.Combine(path, directory), true);
				}
			}
			catch (System.IO.IOException)
			{
				Console.WriteLine("Failed to clean solution directory");
			}
		}

		static Solution ReadConfigFiles(string path)
		{
			try
			{
				// Find solution and project config files in the given directory
				string[] solutionFiles = System.IO.Directory.GetFiles(path, "*.vs_solution_config");
				string[] projectFiles = System.IO.Directory.GetFiles(path, "*.vs_project_config");

				if (solutionFiles.Length != 1)
				{
					Console.WriteLine("Please provide exactly one .vs_solution_config file");
					return null;
				}

				char[] delimeter = { ';' };

				Solution solution = new Solution();

				{
					// Read the solution data
					System.IO.StreamReader solutionReader = new System.IO.StreamReader(solutionFiles[0]);

					solution.Name = solutionReader.ReadLine();
					solution.RootSourceDirectory = solutionReader.ReadLine();
				}

				// Read each project
				foreach (string projectFile in projectFiles)
				{
					System.IO.StreamReader projectReader = new System.IO.StreamReader(projectFile);

					string projectName = projectReader.ReadLine();
					Project project = solution.Projects.Find(p => p.Name.Equals(projectName));

					if (project == null)
					{
						project = new Project();
						project.Name = projectName;
						project.Guid = Guid.NewGuid();
						solution.Projects.Add(project);
					}

					ProjectConfigurationPlatform pcp = new ProjectConfigurationPlatform();
					pcp.Configuration = projectReader.ReadLine();
					pcp.Platform = projectReader.ReadLine();
					pcp.BuildCommand = projectReader.ReadLine();
					pcp.CleanCommand = projectReader.ReadLine();
					pcp.RebuildCommand = projectReader.ReadLine();
					pcp.OutputDir = projectReader.ReadLine();

					string[] defines = RemoveIfSingleEmptyElement(projectReader.ReadLine().Split(delimeter));
					pcp.Defines.AddRange(defines);

					string[] sourceFiles = RemoveIfSingleEmptyElement(projectReader.ReadLine().Split(delimeter));
					foreach (string sourceFile in sourceFiles)
					{
						string fixedUpSourceFile = sourceFile.Replace('/', '\\');
						if (project.RawSourceFiles.Find(x => (x == fixedUpSourceFile)) == null)
						{
							project.RawSourceFiles.Add(fixedUpSourceFile);
						}
					}

					string[] includeDirectories = RemoveIfSingleEmptyElement(projectReader.ReadLine().Split(delimeter));
					pcp.IncludeDirectories.AddRange(includeDirectories);

					pcp.Is64Bit = projectReader.ReadLine().Equals("1");
					pcp.IsDebug = projectReader.ReadLine().Equals("1");

					project.ProjectConfigurationPlatforms.Add(pcp);
				}

				// Add ConfigurationPlatforms to the solution
				foreach (Project project in solution.Projects)
				{
					foreach (ProjectConfigurationPlatform pcp in project.ProjectConfigurationPlatforms)
					{
						// Determine if this combination already exists in the solution
						bool found = solution.SolutionConfigurationPlatforms.Find(
							scp => scp.ConfigurationPlatform.Equals(pcp.ConfigurationPlatform)) != null;

						if (!found)
						{
							SolutionConfigurationPlatform scp = new SolutionConfigurationPlatform();
							scp.Configuration = pcp.Configuration;
							scp.Platform = pcp.Platform;
							solution.SolutionConfigurationPlatforms.Add(scp);
						}
					}
				}

				return solution;
			}
			catch (System.IO.IOException)
			{
				return null;
			}
		}

		static string[] RemoveIfSingleEmptyElement(string[] values)
		{
			if (values.Length == 1 && values[0].Length == 0)
			{
				return new string[0];
			}
			else
			{
				return values;
			}
		}

		static void AddIndividualFilesToProjects(Solution solution)
		{
			foreach (Project project in solution.Projects)
			{
				foreach (string file in project.RawSourceFiles)
				{
					if (file.EndsWith(".c") || file.EndsWith(".cpp"))
					{
						project.SourceFiles.Add(file);
					}
					else if (file.EndsWith(".h"))
					{
						project.IncludeFiles.Add(file);
					}
					else if (file.EndsWith(".inl"))
					{
						project.InlineFiles.Add(file);
					}
					else if (file.EndsWith(".txt"))
					{
						project.TextFiles.Add(file);
					}
					else if (file.EndsWith(".natvis"))
					{
						project.NatvisFiles.Add(file);
					}
					else
					{
						// Unknown file?
					}

					// For each root source path, also add all paths above it in the hierarchy
					string currentPath = System.IO.Path.GetDirectoryName(file);
					while (currentPath.Length > 0)
					{
						if (project.Directories.Find(x => (x == currentPath)) == null)
						{
							project.Directories.Add(currentPath);
						}

						int pos = Math.Max(0, currentPath.LastIndexOf("\\"));
						currentPath = currentPath.Substring(0, pos);
					}
				}
			}
		}

		static void WriteSolution(string path, Solution solution)
		{
			System.IO.StreamWriter file = new System.IO.StreamWriter(path);

			try
			{
				// Write header info
				file.Write("Microsoft Visual Studio Solution File, Format Version ");
				file.Write(Constants.FormatVersion);
				file.WriteLine();

				file.Write("VisualStudioVersion = ");
				file.Write(Constants.VisualStudioVersion);
				file.WriteLine();

				file.Write("MinimumVisualStudioVersion = ");
				file.Write(Constants.MinimumVisualStudioVersion);
				file.WriteLine();

				// Write out projects
				foreach (Project project in solution.Projects)
				{
					file.Write("Project(\"");
					file.Write(Constants.ProjectTypeGuid.ToString("B"));
					file.Write("\") = \"");
					file.Write(project.Name);
					file.Write("\", \"");
					file.Write(project.Name);
					file.Write("\\");
					file.Write(project.Name);
					file.Write(".vcxproj\", \"");
					file.Write(project.Guid.ToString("B"));
					file.Write("\"");
					file.WriteLine();

					file.Write("EndProject");
					file.WriteLine();
				}

				// Global
				file.Write("Global");
				file.WriteLine();

				int indent = 0;
				{
					indent++;

					Indent(file, indent);
					file.Write("GlobalSection(SolutionConfigurationPlatforms) = preSolution");
					file.WriteLine();

					{
						indent++;

						foreach (SolutionConfigurationPlatform scp in solution.SolutionConfigurationPlatforms)
						{
							Indent(file, indent);
							file.Write(scp.ConfigurationPlatform);
							file.Write(" = ");
							file.Write(scp.ConfigurationPlatform);
							file.WriteLine();
						}

						indent--;
					}

					Indent(file, indent);
					file.Write("EndGlobalSection");
					file.WriteLine();

					Indent(file, indent);
					file.Write("GlobalSection(ProjectConfigurationPlatforms) = postSolution");
					file.WriteLine();

					{
						indent++;

						foreach (Project project in solution.Projects)
						{
							foreach (SolutionConfigurationPlatform scp in solution.SolutionConfigurationPlatforms)
							{
								Indent(file, indent);
								file.Write(project.Guid.ToString("B"));
								file.Write(".");
								file.Write(scp.ConfigurationPlatform);
								file.Write(".ActiveCfg = ");
								file.Write(scp.ConfigurationPlatform);
								file.WriteLine();

								// Determine if the project supports this configuration and platform
								bool found = project.ProjectConfigurationPlatforms.Find(
									pcp => pcp.ConfigurationPlatform.Equals(scp.ConfigurationPlatform)) != null;

								if (found)
								{
									Indent(file, indent);
									file.Write(project.Guid.ToString("B"));
									file.Write(".");
									file.Write(scp.ConfigurationPlatform);
									file.Write(".Build.0 = ");
									file.Write(scp.ConfigurationPlatform);
									file.WriteLine();
								}
							}
						}

						indent--;
					}

					Indent(file, indent);
					file.Write("EndGlobalSection");
					file.WriteLine();

					Indent(file, indent);
					file.Write("GlobalSection(SolutionProperties) = preSolution");
					file.WriteLine();

					{
						indent++;

						Indent(file, indent);
						file.Write("HideSolutionNode = FALSE");
						file.WriteLine();

						indent--;
					}

					Indent(file, indent);
					file.Write("EndGlobalSection");
					file.WriteLine();

					indent--;
				}

				file.Write("EndGlobal");
				file.WriteLine();
			}
			finally
			{
				file.Close();
			}
		}

		static void Indent(System.IO.StreamWriter file, int count)
		{
			for (int i = 0; i < count; i++)
			{
				file.Write("\t");
			}
		}

		static void WriteProject(string path, Solution solution, Project project)
		{
			XmlWriterSettings settings = new XmlWriterSettings();
			settings.Indent = true;
			XmlWriter writer = XmlWriter.Create(path, settings);
			XmlWriter filtersWriter = XmlWriter.Create(path + ".filters", settings);

			try
			{
				writer.WriteStartDocument();
				filtersWriter.WriteStartDocument();

				{
					writer.WriteStartElement("Project", Constants.XmlNamespace);
					writer.WriteAttributeString("DefaultTargets", "Build");
					writer.WriteAttributeString("ToolsVersion", Constants.ToolsVersion);

					// List all the configurations
					{
						writer.WriteStartElement("ItemGroup");
						writer.WriteAttributeString("Label", "ProjectConfigurations");

						foreach (ProjectConfigurationPlatform pcp in project.ProjectConfigurationPlatforms)
						{
							writer.WriteStartElement("ProjectConfiguration");
							writer.WriteAttributeString("Include", pcp.ConfigurationPlatform);
							writer.WriteElementString("Configuration", pcp.Configuration);
							writer.WriteElementString("Platform", pcp.Platform);
							writer.WriteEndElement();
						}

						writer.WriteEndElement();
					}

					// Disable platform checks since we're just redirecting to makefiles
					//{
					//	writer.WriteStartElement("PropertyGroup");
					//	writer.WriteElementString("PlatformTargetsFound", "True");
					//	writer.WriteEndElement();
					//}

					// Globals
					{
						writer.WriteStartElement("PropertyGroup");
						writer.WriteAttributeString("Label", "Globals");
						writer.WriteElementString("ProjectGuid", project.Guid.ToString("B"));
						writer.WriteElementString("RootNamespace", project.Name);
						writer.WriteElementString("Keyword", Constants.MakeFileProjKeyword);
						writer.WriteEndElement();
					}

					AddElementWithAttribute(
						writer, "Import", "Project", @"$(VCTargetsPath)\Microsoft.Cpp.Default.props");

					// Configuration settings
					foreach (ProjectConfigurationPlatform pcp in project.ProjectConfigurationPlatforms)
					{
						writer.WriteStartElement("PropertyGroup");
						writer.WriteAttributeString("Condition", pcp.ConditionString);
						writer.WriteAttributeString("Label", "Configuration");

						{
							writer.WriteElementString("ConfigurationType", Constants.ConfigurationType);
							string useDebugLibrariesString = pcp.IsDebug ? "true" : "false";
							writer.WriteElementString("UseDebugLibraries", useDebugLibrariesString);
							writer.WriteElementString("PlatformToolset", Constants.PlatformToolset);
						}

						writer.WriteEndElement();
					}

					{
						writer.WriteStartElement("Import");
						writer.WriteAttributeString("Project", @"$(VCTargetsPath)\Microsoft.Cpp.props");
						writer.WriteEndElement();
					}

					// Extension settings
					AddElementWithAttribute(writer, "ImportGroup", "Label", "ExtensionSettings");

					// Property sheets
					foreach (ProjectConfigurationPlatform pcp in project.ProjectConfigurationPlatforms)
					{
						writer.WriteStartElement("ImportGroup");
						writer.WriteAttributeString("Label", "PropertySheets");
						writer.WriteAttributeString("Condition", pcp.ConditionString);

						{
							writer.WriteStartElement("Import");
							writer.WriteAttributeString("Project",
								@"$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props");
							writer.WriteAttributeString("Condition",
								@"exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')");
							writer.WriteAttributeString("Label", "LocalAppDataPlatform");
							writer.WriteEndElement();
						}

						writer.WriteEndElement();
					}

					// User macros
					AddElementWithAttribute(writer, "PropertyGroup", "Label", "UserMacros");

					// Configuration directories
					foreach (ProjectConfigurationPlatform pcp in project.ProjectConfigurationPlatforms)
					{
						writer.WriteStartElement("PropertyGroup");
						writer.WriteAttributeString("Condition", pcp.ConditionString);

						{
							// Makefile commands
							writer.WriteElementString("NMakeBuildCommandLine", pcp.BuildCommand);
							writer.WriteElementString("NMakeRebuildCommandLine", pcp.RebuildCommand);
							writer.WriteElementString("NMakeCleanCommandLine", pcp.CleanCommand);
							writer.WriteElementString("NMakeOutput", pcp.OutputDir);
						}

						{
							// Preprocessor definitions
							StringBuilder defines = new StringBuilder();

							pcp.Defines.ForEach(define => AppendToPropertyList(defines, define));
							foreach (string define in Constants.DefaultDefines)
							{
								AppendToPropertyList(defines, define);
							}

							writer.WriteElementString("NMakePreprocessorDefinitions", defines.ToString());
						}

						{
							// Include directories
							StringBuilder includes = new StringBuilder();

							pcp.IncludeDirectories.ForEach(
								include => AppendToPropertyList(includes, include));
							foreach (string include in Constants.DefaultIncludeDirectories)
							{
								AppendToPropertyList(includes, include);
							}

							writer.WriteElementString("NMakeIncludeSearchPath", includes.ToString());
						}

						writer.WriteEndElement();
					}

					// File listing
					{
						// Set up the filters
						filtersWriter.WriteStartElement("Project", Constants.XmlNamespace);
						filtersWriter.WriteAttributeString("ToolsVersion", Constants.FiltersToolsVersion);

						{
							filtersWriter.WriteStartElement("ItemGroup");

							foreach (string dir in project.Directories)
							{
								filtersWriter.WriteStartElement("Filter");
								filtersWriter.WriteAttributeString("Include", dir);
								filtersWriter.WriteElementString("UniqueIdentifier", Guid.NewGuid().ToString("B"));
								filtersWriter.WriteElementString("Extensions", "*");
								filtersWriter.WriteEndElement();
							}

							filtersWriter.WriteEndElement();
						}

						AddFilesToProject(writer, filtersWriter, solution.RootSourceDirectory,
							project.IncludeFiles, "ClInclude");
						AddFilesToProject(writer, filtersWriter, solution.RootSourceDirectory,
							project.SourceFiles, "ClCompile");
						AddFilesToProject(writer, filtersWriter, solution.RootSourceDirectory,
							project.InlineFiles, "None");
						AddFilesToProject(writer, filtersWriter, solution.RootSourceDirectory,
							project.TextFiles, "Text");
						AddFilesToProject(writer, filtersWriter, solution.RootSourceDirectory,
							project.NatvisFiles, "Natvis");

						filtersWriter.WriteEndElement();
					}

					AddElementWithAttribute(writer, "Import", "Project", @"$(VCTargetsPath)\Microsoft.Cpp.targets");

					// Extension targets
					AddElementWithAttribute(writer, "ImportGroup", "Label", "ExtensionTargets");

					writer.WriteEndElement();
				}

				writer.WriteEndDocument();
				filtersWriter.WriteEndDocument();
			}
			finally
			{
				writer.Close();
				filtersWriter.Close();
			}
		}

		static void AddElementWithAttribute(XmlWriter writer, string element, string attribute, string value)
		{
			writer.WriteStartElement(element);
			if (attribute != null)
			{
				writer.WriteAttributeString(attribute, value);
			}
			writer.WriteEndElement();
		}

		static void AddFilesToProject(XmlWriter writer, XmlWriter filtersWriter, string rootSourceDirectory,
			List<string> files, string type)
		{
			writer.WriteStartElement("ItemGroup");
			filtersWriter.WriteStartElement("ItemGroup");

			{
				foreach (string file in files)
				{
					// The first .. is to step from project root to solution root
					string filePath = System.IO.Path.Combine("..", rootSourceDirectory, file);
					int lastDirSeparator = file.LastIndexOf('\\');
					lastDirSeparator = Math.Max(0, lastDirSeparator);
					string filter = file.Substring(0, lastDirSeparator);

					writer.WriteStartElement(type);
					writer.WriteAttributeString("Include", filePath);
					writer.WriteEndElement();

					filtersWriter.WriteStartElement(type);
					filtersWriter.WriteAttributeString("Include", filePath);
					filtersWriter.WriteElementString("Filter", filter);
					filtersWriter.WriteEndElement();
				}
			}

			writer.WriteEndElement();
			filtersWriter.WriteEndElement();
		}

		static void AppendToPropertyList(StringBuilder propertyList, string value)
		{
			if (propertyList.Length > 0)
			{
				propertyList.Append(';');
			}

			propertyList.Append(value);
		}
	}
}
