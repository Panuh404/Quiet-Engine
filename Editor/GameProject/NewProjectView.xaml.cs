using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace Editor.GameProject
{
    /// <summary>
    /// Interaction logic for CreateProjectView.xaml
    /// </summary>
    public partial class NewProjectView : UserControl
    {
        public NewProjectView()
        {
            InitializeComponent();

        }

        private void OnCreate_ButtonClick(object sender, RoutedEventArgs e)
        {
            var dc = DataContext as NewProject;
            var projectPath = dc.CreateProject(TemplateListBox.SelectedItem as ProjectTemplate);
            bool dialogResult = false;

            var window = Window.GetWindow(this);
            if (!string.IsNullOrEmpty(projectPath))
            {
                dialogResult = true;
            }

            window.DialogResult = dialogResult;
            window.Close();
        }
    }
}
