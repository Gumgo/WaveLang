using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace VsProjectGenerator
{
    // $TODO by default, the source project still tries to build for some reason even though it doesn't have Build.0,
    // need to fix this
    // $TODO make the obj files redirect to another folder because it's inconvenient to do a full rebuild upon sln regen

    class Constants
    {
        public static readonly Guid ProjectTypeGuid = new Guid("8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942"); // GUID for C++

        public const string FormatVersion = "12.00";
        public const string VisualStudioVersion = "14.0.24720.0";
        public const string MinimumVisualStudioVersion = "10.0.40219.1";

        public const string ToolsVersion = "14.0";
        public const string FiltersToolsVersion = "4.0";
        public const string XmlNamespace = "http://schemas.microsoft.com/developer/msbuild/2003";
        public const string ConfigurationType = "Application";
        public const string PlatformToolset = "v140";
        public const string CharacterSet = "MultiByte";

        public const string WarningLevel = "Level3";

        public const string SolutionName = "WaveLang";
        public const string SolutionDirectory = "VsProjectFiles";
        public const string SolutionDirectoryToRoot = @"$(SolutionDir)..\";
        public const string ProjectDirectoryToRoot = @"..\..\";
        public const string SourcePath = @"source\";
        public const string BinaryPath = @"bin\";

        public static readonly string[] DefaultIncludePaths =
            { "$(VC_IncludePath)", "$(WindowsSDK_IncludePath)" };
        public static readonly string[] DefaultLibraryPaths32 =
            { "$(VC_LibraryPath_x86)", "$(WindowsSDK_LibraryPath_x86)" };
        public static readonly string[] DefaultLibraryPaths64 =
            { }; // Fill in if we target 64-bit platforms

        public const string DebugConfiguration = "Debug";
        public const string ReleaseConfiguration = "Release";
        public const string Platform32 = "Win32";
    }

    class Solution
    {
        public Solution()
        {
            Projects = new List<Project>();
            SolutionConfigurationPlatforms = new List<SolutionConfigurationPlatform>();
        }

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
            SourcePaths = new List<string>();
            ProjectConfigurationPlatforms = new List<ProjectConfigurationPlatform>();
            Directories = new List<string>();
            SourceFiles = new List<string>();
            IncludeFiles = new List<string>();
            InlineFiles = new List<string>();
            TextFiles = new List<string>();
            NatvisFiles = new List<string>();
        }

        public string Name { get; set; }
        public Guid Guid { get; set; }
        public bool BuildEnabled { get; set; }

        public List<string> SourcePaths { get; set; }
        public List<ProjectConfigurationPlatform> ProjectConfigurationPlatforms;

        public List<string> Directories { get; set; }
        public List<string> SourceFiles { get; set; }
        public List<string> IncludeFiles { get; set; }
        public List<string> InlineFiles { get; set; }
        public List<string> TextFiles { get; set; }
        public List<string> NatvisFiles { get; set; }
    }

    class ProjectConfigurationPlatform
    {
        public ProjectConfigurationPlatform()
        {
            IncludePaths = new List<string>();
            LibraryPaths = new List<string>();
        }

        public string Configuration { get; set; }
        public string Platform { get; set; }

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

        public bool Is64Bit { get; set; }
        public bool IsDebug { get; set; }

        public List<string> IncludePaths { get; set; }
        public List<string> LibraryPaths { get; set; }
    }

    class Program
    {
        static void Main(string[] args)
        {
            try
            {
                if (args.Length != 1)
                {
                    throw new ArgumentException("Invalid argument count");
                }

                string path = args[0];
                Solution solution = ReadConfigFile(path);

                AddIndividualFilesToProjects(solution);

                if (System.IO.Directory.Exists(Constants.SolutionDirectory))
                {
                    System.IO.Directory.Delete(Constants.SolutionDirectory, true);
                }
                System.IO.Directory.CreateDirectory(Constants.SolutionDirectory);

                WriteSolution(Constants.SolutionDirectory + "\\" + Constants.SolutionName + ".sln", solution);
                foreach (Project project in solution.Projects)
                {
                    System.IO.Directory.CreateDirectory(Constants.SolutionDirectory + "\\" + project.Name);
                    WriteProject(Constants.SolutionDirectory + "\\" +
                        project.Name + "\\" + project.Name + ".vcxproj", project);
                }
            }
            catch (Exception e)
            {
                Console.WriteLine(e.Message);
            }
        }

        static Solution ReadConfigFile(string path)
        {
            Solution solution = new Solution();

            {
                SolutionConfigurationPlatform scp = new SolutionConfigurationPlatform();
                scp.Configuration = Constants.DebugConfiguration;
                scp.Platform = Constants.Platform32;
                solution.SolutionConfigurationPlatforms.Add(scp);
            }

            {
                SolutionConfigurationPlatform scp = new SolutionConfigurationPlatform();
                scp.Configuration = Constants.ReleaseConfiguration;
                scp.Platform = Constants.Platform32;
                solution.SolutionConfigurationPlatforms.Add(scp);
            }

            XmlDocument config = new XmlDocument();
            config.Load(path);

            // Create a project for all source files
            Project sourceProject = new Project();
            sourceProject.Guid = Guid.NewGuid();
            sourceProject.Name = "source";
            sourceProject.BuildEnabled = false;
            sourceProject.ProjectConfigurationPlatforms.Add(InitializeProjectConfigurationPlatform(true));
            sourceProject.ProjectConfigurationPlatforms.Add(InitializeProjectConfigurationPlatform(false));
            sourceProject.SourcePaths.Add("");
            solution.Projects.Add(sourceProject);

            // The source project has access to includes from all other projects
            List<string> allIncludes = new List<string>();

            XmlNodeList projectNodes = config.GetElementsByTagName("Projects");
            if (projectNodes.Count != 1)
            {
                throw new XmlException("Single Projects element expected");
            }

            XmlNode projectsNode = projectNodes.Item(0);
            foreach (XmlNode projectNode in projectsNode.ChildNodes)
            {
                if (projectNode.Name != "Project")
                {
                    throw new XmlException("Unexpected project node");
                }

                Project project = new Project();
                project.Guid = Guid.NewGuid();
                project.BuildEnabled = true;
                ProjectConfigurationPlatform pcpDebug = InitializeProjectConfigurationPlatform(true);
                ProjectConfigurationPlatform pcpRelease = InitializeProjectConfigurationPlatform(false);
                project.ProjectConfigurationPlatforms.Add(pcpDebug);
                project.ProjectConfigurationPlatforms.Add(pcpRelease);

                project.Name = projectNode.Attributes["Name"].Value;

                foreach (XmlNode child in projectNode.ChildNodes)
                {
                    if (child.Name == "SourceDir")
                    {
                        string sourcePath = TrimTrailingBackslash(child.Attributes["Path"].Value) + "\\";
                        project.SourcePaths.Add(sourcePath);
                    }
                    else if (child.Name == "IncludeDir")
                    {
                        string includePath = TrimTrailingBackslash(child.Attributes["Path"].Value) + "\\";
                        pcpDebug.IncludePaths.Add(includePath);
                        pcpRelease.IncludePaths.Add(includePath);

                        if (allIncludes.Find(x => (x == includePath)) == null)
                        {
                            allIncludes.Add(includePath);
                        }
                    }
                    else if (child.Name == "DebugLibraryDir")
                    {
                        string debugLibraryDir = TrimTrailingBackslash(child.Attributes["Path"].Value) + "\\";
                        pcpDebug.LibraryPaths.Add(debugLibraryDir);
                    }
                    else if (child.Name == "ReleaseLibraryDir")
                    {
                        string releaseLibraryDir = TrimTrailingBackslash(child.Attributes["Path"].Value) + "\\";
                        pcpRelease.LibraryPaths.Add(releaseLibraryDir);
                    }
                }

                solution.Projects.Add(project);
            }

            // Add all includes to the source project
            foreach (ProjectConfigurationPlatform pcp in sourceProject.ProjectConfigurationPlatforms)
            {
                foreach (string include in allIncludes)
                {
                    pcp.IncludePaths.Add(include);
                }
            }

            return solution;
        }

        static void AddIndividualFilesToProjects(Solution solution)
        {
            foreach (Project project in solution.Projects)
            {
                foreach (string sourcePath in project.SourcePaths)
                {
                    string fullSourcePath = Constants.SourcePath + sourcePath;
                    string[] extensions = { ".cpp", ".h", ".inl", ".txt", ".natvis" };
                    List<string> allFiles = new List<string>();
                    EnumerateFilesRecursively(fullSourcePath, extensions, allFiles, project.Directories);

                    foreach (string file in allFiles)
                    {
                        string trimmedFile = file.Substring(Constants.SourcePath.Length);

                        if (file.EndsWith(".cpp"))
                        {
                            project.SourceFiles.Add(trimmedFile);
                        }
                        else if (file.EndsWith(".h"))
                        {
                            project.IncludeFiles.Add(trimmedFile);
                        }
                        else if (file.EndsWith(".inl"))
                        {
                            project.InlineFiles.Add(trimmedFile);
                        }
                        else if (file.EndsWith(".txt"))
                        {
                            project.TextFiles.Add(trimmedFile);
                        }
                        else if (file.EndsWith(".natvis"))
                        {
                            project.NatvisFiles.Add(trimmedFile);
                        }
                        else
                        {
                            // Unknown file?
                        }
                    }
                }

                // For each root source path, also add all paths above it in the hierarchy
                string trimmedSourcePath = TrimTrailingBackslash(Constants.SourcePath);
                foreach (string sourcePath in project.SourcePaths)
                {
                    string currentPath = TrimTrailingBackslash(Constants.SourcePath + sourcePath);
                    while (currentPath != trimmedSourcePath && currentPath.Length > 0)
                    {
                        int pos = Math.Max(0, currentPath.LastIndexOf("\\"));
                        currentPath = currentPath.Substring(0, pos);
                        if (currentPath.Length > 0)
                        {
                            if (project.Directories.Find(x => (x == currentPath)) == null)
                            {
                                project.Directories.Add(currentPath);
                            }
                        }
                    }
                }
            }
        }

        static void EnumerateFilesRecursively(string path, string[] extensions,
            List<string> outFiles, List<string> outDirs)
        {
            string trimmedPath = TrimTrailingBackslash(path);
            outDirs.Add(trimmedPath);

            string[] files = System.IO.Directory.GetFiles(path);

            foreach (string file in files)
            {
                foreach (string extension in extensions)
                {
                    if (file.EndsWith(extension))
                    {
                        outFiles.Add(file);
                        break;
                    }
                }
            }

            foreach (string dir in System.IO.Directory.GetDirectories(path))
            {
                EnumerateFilesRecursively(dir, extensions, outFiles, outDirs);
            }
        }

        static string TrimTrailingBackslash(string path)
        {
            if (path.EndsWith("\\"))
            {
                return path.Substring(0, path.Length - 1);
            }
            else
            {
                return path;
            }
        }

        static ProjectConfigurationPlatform InitializeProjectConfigurationPlatform(bool debug)
        {
            ProjectConfigurationPlatform pcp = new ProjectConfigurationPlatform();
            pcp.Configuration = debug ? Constants.DebugConfiguration : Constants.ReleaseConfiguration;
            pcp.Platform = Constants.Platform32;
            pcp.Is64Bit = false;
            pcp.IsDebug = debug;
            pcp.IncludePaths = new List<string>();
            pcp.LibraryPaths = new List<string>();
            return pcp;
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

                                if (project.BuildEnabled)
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

        static void WriteProject(string path, Project project)
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

                    // Globals
                    {
                        writer.WriteStartElement("PropertyGroup");
                        writer.WriteAttributeString("Label", "Globals");
                        writer.WriteElementString("ProjectGuid", project.Guid.ToString("B"));
                        writer.WriteElementString("RootNamespace", project.Name);
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
                            writer.WriteElementString("UseDebugLibraries", pcp.IsDebug ? "true" : "false");
                            writer.WriteElementString("PlatformToolset", Constants.PlatformToolset);

                            if (!pcp.IsDebug)
                            {
                                writer.WriteElementString("WholeProgramOptimization", "true");
                            }

                            writer.WriteElementString("CharacterSet", Constants.CharacterSet);
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
                        writer.WriteElementString("OutDir", Constants.SolutionDirectoryToRoot + Constants.BinaryPath);

                        {
                            // Include directories
                            StringBuilder builder = new StringBuilder();
                            foreach (string includePath in Constants.DefaultIncludePaths)
                            {
                                builder.Append(includePath);
                                builder.Append(";");
                            }

                            builder.Append(Constants.SolutionDirectoryToRoot + Constants.SourcePath);
                            builder.Append(";");

                            foreach (string includePath in pcp.IncludePaths)
                            {
                                builder.Append(includePath);
                                builder.Append(";");
                            }

                            // Remove trailing ;
                            if (builder.Length > 0)
                            {
                                builder.Remove(builder.Length - 1, 1);
                            }

                            writer.WriteElementString("IncludePath", builder.ToString());
                        }

                        {
                            // Library directories
                            StringBuilder builder = new StringBuilder();
                            string[] defaultLibraryPaths = pcp.Is64Bit ?
                                Constants.DefaultLibraryPaths64 : Constants.DefaultLibraryPaths32;
                            foreach (string libraryPath in defaultLibraryPaths)
                            {
                                builder.Append(libraryPath);
                                builder.Append(";");
                            }

                            foreach (string libraryPath in pcp.LibraryPaths)
                            {
                                builder.Append(libraryPath);
                                builder.Append(";");
                            }

                            // Remove trailing ;
                            if (builder.Length > 0)
                            {
                                builder.Remove(builder.Length - 1, 1);
                            }

                            writer.WriteElementString("LibraryPath", builder.ToString());
                        }

                        writer.WriteEndElement();
                    }

                    // Compiler settings
                    foreach (ProjectConfigurationPlatform pcp in project.ProjectConfigurationPlatforms)
                    {
                        writer.WriteStartElement("ItemDefinitionGroup");
                        writer.WriteAttributeString("Condition", pcp.ConditionString);

                        {
                            writer.WriteStartElement("ClCompile");
                            writer.WriteElementString("WarningLevel", Constants.WarningLevel);
                            writer.WriteElementString("Optimization", pcp.IsDebug ? "Disabled" : "MaxSpeed");

                            if (!pcp.IsDebug)
                            {
                                writer.WriteElementString("FunctionLevelLinking", "true");
                                writer.WriteElementString("IntrinsicFunctions", "true");
                            }

                            writer.WriteElementString("SDLCheck", "true");
                            writer.WriteEndElement();
                        }

                        {
                            writer.WriteStartElement("Link");
                            writer.WriteElementString("GenerateDebugInformation", "true");

                            if (!pcp.IsDebug)
                            {
                                writer.WriteElementString("EnableCOMDATFolding", "true");
                                writer.WriteElementString("OptimizeReferences", "true");
                            }

                            writer.WriteEndElement();
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

                        AddFilesToProject(writer, filtersWriter, project.IncludeFiles, "ClInclude");
                        AddFilesToProject(writer, filtersWriter, project.SourceFiles, "ClCompile");
                        AddFilesToProject(writer, filtersWriter, project.InlineFiles, "None");
                        AddFilesToProject(writer, filtersWriter, project.TextFiles, "Text");
                        AddFilesToProject(writer, filtersWriter, project.NatvisFiles, "Natvis");

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

        static void AddFilesToProject(XmlWriter writer, XmlWriter filtersWriter, List<string> files, string type)
        {
            writer.WriteStartElement("ItemGroup");
            filtersWriter.WriteStartElement("ItemGroup");

            {
                foreach (string file in files)
                {
                    string filePath = Constants.ProjectDirectoryToRoot + Constants.SourcePath + file;
                    int lastDirSeparator = filePath.LastIndexOf('\\');
                    lastDirSeparator = Math.Max(0, lastDirSeparator);
                    string dir = filePath.Substring(0, lastDirSeparator);
                    dir = filePath.Substring(
                        Constants.ProjectDirectoryToRoot.Length, dir.Length - Constants.ProjectDirectoryToRoot.Length);

                    writer.WriteStartElement(type);
                    writer.WriteAttributeString("Include", filePath);
                    writer.WriteEndElement();

                    filtersWriter.WriteStartElement(type);
                    filtersWriter.WriteAttributeString("Include", filePath);
                    filtersWriter.WriteElementString("Filter", dir);
                    filtersWriter.WriteEndElement();
                }
            }

            writer.WriteEndElement();
            filtersWriter.WriteEndElement();
        }
    }
}
