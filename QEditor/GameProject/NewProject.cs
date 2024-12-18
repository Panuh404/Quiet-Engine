﻿using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Windows.Shapes;
using QEditor.Utilities;
using Path = System.IO.Path;

namespace QEditor.GameProject
{
    [DataContract]
    public class ProjectTemplate
    {
        [DataMember]
        public string ProjectType { get; set; }
        [DataMember]
        public string ProjectFile { get; set; }
        [DataMember]
        public List<string> Folders { get; set; }

        public byte[] Icon { get; set; }
        public byte[] Screenshot { get; set; }

        public string IconFilePath { get; set; }
        public string ScreenshotFilePath { get; set; }
        public string ProjectFilePath { get; set; }
        public string TemplatePath { get; set; }
    }

    class NewProject : ViewModelBase
    {
        // TODO: get the path from the installation location
        private readonly string _templatePath = @"..\..\QEditor\Templates";

        private string _projectName = "NewProject";
        public string ProjectName
        {
            get => _projectName; 
            set
            {
                if (_projectName != value)
                {
                    _projectName = value;
                    ValidateProjectPath();
                    OnPropertyChanged(nameof(ProjectName));
                }
            }
        }
 
        private string _projectPath = $@"{Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments)}\Quiet Projects\";
        public string ProjectPath
        {
            get => _projectPath;
            set
            {
                if (_projectPath != value)
                {
                    _projectPath = value;
                    ValidateProjectPath();
                    OnPropertyChanged(nameof(ProjectPath));
                }
            }
        }

        private bool _isValid;
        public bool IsValid
        {
            get => _isValid;
            set
            {
                if (_isValid != value)
                {
                    _isValid = value;
                    OnPropertyChanged(nameof(IsValid));
                }
            }
        }

        private string _errorMsg;
        public string ErrorMsg
        {
            get => _errorMsg;
            set
            {
                if (_errorMsg != value)
                {
                    _errorMsg = value;
                    OnPropertyChanged(nameof(ErrorMsg));
                }
            }
        }

        private ObservableCollection<ProjectTemplate> _projectTemplates = new ObservableCollection<ProjectTemplate>();
        public ReadOnlyObservableCollection<ProjectTemplate> ProjectTemplates
        { get; }

        private bool ValidateProjectPath()
        {
            var path = ProjectPath;
            if (!Path.EndsInDirectorySeparator(path)) path += @"\";
            path += $@"{ProjectName}\";

            IsValid = false;
            if (string.IsNullOrEmpty(ProjectName.Trim()))                                       // Check for null Name
            {
                ErrorMsg = "Type in a project name";
            }
            else if (ProjectName.IndexOfAny(Path.GetInvalidFileNameChars()) != -1)              // Check for invalid Name
            {
                ErrorMsg = "Invalid character(s) used in project name";
            }
            else if (string.IsNullOrEmpty(ProjectPath.Trim()))                                  // Check for null Path
            {
                ErrorMsg = "Select a valid project folder";
            }
            else if (ProjectPath.IndexOfAny(Path.GetInvalidPathChars()) != -1)                  // Check for invalid Path
            {
                ErrorMsg = "Invalid character(s) used in project name";
            }
            else if (Directory.Exists(path) && Directory.EnumerateFileSystemEntries(path).Any()) // Check for existing Path
            {
                ErrorMsg = "Selected project folder already exists and is not empty";
            }
            else
            {
                ErrorMsg = string.Empty;
                IsValid = true;
            }
            return IsValid;
        }

        public string CreateProject(ProjectTemplate template)
        {
            ValidateProjectPath();
            if (!IsValid)
            {
                return string.Empty;
            }

            if (!Path.EndsInDirectorySeparator(ProjectPath)) ProjectPath += @"\";
            var path = $@"{ProjectPath}{ProjectName}\";

            try
            {
                if (!Directory.Exists(path)) Directory.CreateDirectory(path);   // Create main project folder if is null
                foreach (var folder in template.Folders)
                {
                    Directory.CreateDirectory(Path.GetFullPath(Path.Combine(Path.GetDirectoryName(path), folder))); // Create project folders
                }
                var dirInfo = new DirectoryInfo(path + @".quiet\"); 
                dirInfo.Attributes |= FileAttributes.Hidden;
                File.Copy(template.IconFilePath, Path.GetFullPath(Path.Combine(dirInfo.FullName, "Icon.png")));
                File.Copy(template.ScreenshotFilePath, Path.GetFullPath(Path.Combine(dirInfo.FullName, "Screenshot.png")));

                var projectXML = File.ReadAllText(template.ProjectFilePath);
                projectXML = string.Format(projectXML, ProjectName, path);
                var projectPath = Path.GetFullPath(Path.Combine(path, $"{ProjectName}{Project.Extension}"));
                File.WriteAllText(projectPath, projectXML);

                CreateMSVCSolution(template, path);

                return path;
            }
            catch (Exception e)
            {
                Debug.WriteLine(e.Message);
                Logger.Log(MessageType.Error, $"Failed to create {ProjectName}");
                throw;
            }
        }

        private void CreateMSVCSolution(ProjectTemplate template, string projectPath)
        {
            Debug.Assert(File.Exists(Path.Combine(template.TemplatePath, "MSVCSolution")));
            Debug.Assert(File.Exists(Path.Combine(template.TemplatePath, "MSVCProject")));

            var engineApiPath = Path.Combine(MainWindow.QuietPath, @"QEngine\EngineAPI\");
            Debug.Assert(Directory.Exists(engineApiPath));

            var _0 = ProjectName;
            var _1 = "{" + Guid.NewGuid().ToString().ToUpper() + "}";
            var _2 = engineApiPath;
            var _3 = MainWindow.QuietPath;

            var projectType = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}";
            var solutionGuid = "{" + Guid.NewGuid().ToString().ToUpper() + "}";

            // Create sln solution file 
            var solution = File.ReadAllText(Path.Combine(template.TemplatePath, "MSVCSolution"));
            solution = string.Format(solution, _0, _1, solutionGuid, projectType);
            File.WriteAllText(Path.GetFullPath(Path.Combine(projectPath, $"{_0}.sln")), solution);
            
            // Create vcxproj project file
            var project = File.ReadAllText(Path.Combine(template.TemplatePath, "MSVCProject"));
            project = string.Format(project, _0, _1, _2, _3);
            File.WriteAllText(Path.GetFullPath(Path.Combine(projectPath, $@"Code\{_0}.vcxproj")), project);

        }

        public NewProject()
        {
            ProjectTemplates = new ReadOnlyObservableCollection<ProjectTemplate>(_projectTemplates);
            try
            {
                var templateFiles = Directory.GetFiles(_templatePath, "template.xml", SearchOption.AllDirectories);
                Debug.Assert(templateFiles.Any());
                foreach (var file in templateFiles)
                {
                    var template = Serializer.FromFile<ProjectTemplate>(file);
                    template.TemplatePath = Path.GetDirectoryName(file);
                    template.IconFilePath = Path.GetFullPath(Path.Combine(template.TemplatePath, "Icon.png"));
                    template.Icon = File.ReadAllBytes(template.IconFilePath);
                    template.ScreenshotFilePath = Path.GetFullPath(Path.Combine(template.TemplatePath, "Screenshot.png"));
                    template.Screenshot = File.ReadAllBytes(template.ScreenshotFilePath);
                    template.ProjectFilePath = Path.GetFullPath(Path.Combine(template.TemplatePath, template.ProjectFile));

                    _projectTemplates.Add(template);
                }
                ValidateProjectPath();
            }
            catch (Exception e)
            {
                Debug.WriteLine(e.Message);
                Logger.Log(MessageType.Error, $"Failed to read project templates");
                throw;

            }
        }
    }
}
