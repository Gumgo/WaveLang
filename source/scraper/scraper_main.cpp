#include "common/common.h"

#include "scraper/ast_visitor.h"
#include "scraper/native_module_registration_generator.h"
#include "scraper/scraper_result.h"
#include "scraper/task_function_registration_generator.h"

#pragma warning(push, 0) // Disable warnings for LLVM
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#pragma warning(pop)

#include <fstream>
#include <vector>

// Classes required to run clang:

class c_scraper_frontend_action : public clang::ASTFrontendAction {
public:
	c_scraper_frontend_action(c_scraper_result *result) {
		m_result = result;
	}

	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef in_file) {
		return std::unique_ptr<clang::ASTConsumer>(create_ast_visitor(ci, m_result));
	}

private:
	c_scraper_result *m_result;
};

class c_scraper_frontend_action_factory : public clang::tooling::FrontendActionFactory {
public:
	c_scraper_frontend_action_factory(c_scraper_result *result) {
		m_result = result;
	}

	clang::FrontendAction *create() {
		return new c_scraper_frontend_action(m_result);
	}

private:
	c_scraper_result *m_result;
};

int main(int argc, const char **argv) {
	int32 result = 0;

	// This is the resulting data structure we will be filling in
	c_scraper_result scraper_result;

	// Run clang over the input
	{
		llvm::cl::OptionCategory scraper_options_category("scraper options");
		llvm::cl::opt<std::string> output_filename_option(
			"o",
			llvm::cl::desc("Scraper output filename"),
			llvm::cl::value_desc("filename"),
			llvm::cl::Required,
			llvm::cl::cat(scraper_options_category));

		llvm::cl::opt<std::string> native_module_registration_function_name_option(
			"n",
			llvm::cl::desc("Native module registration function name"),
			llvm::cl::value_desc("function name"),
			llvm::cl::Required,
			llvm::cl::cat(scraper_options_category));

		llvm::cl::opt<std::string> task_function_registration_function_name_option(
			"t",
			llvm::cl::desc("Task function registration function name"),
			llvm::cl::value_desc("function name"),
			llvm::cl::Required,
			llvm::cl::cat(scraper_options_category));

		clang::tooling::CommonOptionsParser common_options_parser(argc, argv, scraper_options_category);

		llvm::ArrayRef<std::string> source_path_list(
			&common_options_parser.getSourcePathList().front(),
			common_options_parser.getSourcePathList().size());
		clang::tooling::ClangTool clang_tool(common_options_parser.getCompilations(), source_path_list);

		// Run the AST visitor
		c_scraper_frontend_action_factory action_factory(&scraper_result);
		clang_tool.run(&action_factory);

		if (scraper_result.get_success()) {
			std::ofstream out(output_filename_option.getValue());
			if (!out.is_open()) {
				result = 1;
			} else {
				// Generate header
				out << "// THIS FILE IS AUTO-GENERATED - DO NOT EDIT\n\n";
			}

			if (result == 0 && !generate_native_module_registration(
				&scraper_result,
				native_module_registration_function_name_option.getValue().c_str(),
				out)) {
				result = 1;
			}

			if (result == 0 && !generate_task_function_registration(
				&scraper_result,
				task_function_registration_function_name_option.getValue().c_str(),
				out)) {
				result = 1;
			}

			if (result == 0 && out.fail()) {
				result = 1;
			}
		} else {
			result = 1;
		}
	}

	return result;
}
