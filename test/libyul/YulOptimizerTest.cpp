/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

#include <test/libyul/YulOptimizerTest.h>
#include <test/libyul/YulOptimizerTestCommon.h>

#include <test/libsolidity/util/SoltestErrors.h>
#include <test/libyul/Common.h>
#include <test/Common.h>

#include <libyul/Object.h>
#include <libyul/AsmPrinter.h>
#include <libyul/AST.h>
#include <libyul/YulStack.h>

#include <liblangutil/CharStreamProvider.h>
#include <liblangutil/SourceReferenceFormatter.h>
#include <liblangutil/Scanner.h>

#include <libsolutil/AnsiColorized.h>
#include <libsolutil/StringUtils.h>

#include <fstream>

using namespace solidity;
using namespace solidity::util;
using namespace solidity::langutil;
using namespace solidity::yul;
using namespace solidity::yul::test;
using namespace solidity::frontend;
using namespace solidity::frontend::test;

YulOptimizerTest::YulOptimizerTest(std::string const& _filename):
	EVMVersionRestrictedTestCase(_filename)
{
	boost::filesystem::path path(_filename);

	if (path.empty() || std::next(path.begin()) == path.end() || std::next(std::next(path.begin())) == path.end())
		BOOST_THROW_EXCEPTION(std::runtime_error("Filename path has to contain a directory: \"" + _filename + "\"."));
	m_optimizerStep = std::prev(std::prev(path.end()))->string();

	m_source = m_reader.source();

	auto dialectName = m_reader.stringSetting("dialect", "evm");
	soltestAssert(dialectName == "evm"); // We only have one dialect now

	m_expectation = m_reader.simpleExpectations();
}

TestCase::TestResult YulOptimizerTest::run(std::ostream& _stream, std::string const& _linePrefix, bool const _formatted)
{
	m_object = parse(_stream, _linePrefix, _formatted, m_source);
	if (!m_object)
		return TestResult::FatalError;

	YulOptimizerTestCommon tester(m_object);
	tester.setStep(m_optimizerStep);

	if (!tester.runStep())
	{
		AnsiColorized(_stream, _formatted, {formatting::BOLD, formatting::RED}) << _linePrefix << "Invalid optimizer step: " << m_optimizerStep << std::endl;
		return TestResult::FatalError;
	}
	auto optimizedObject = tester.optimizedObject();
	std::string printedOptimizedObject;
	soltestAssert(optimizedObject->dialect());
	if (optimizedObject->subObjects.empty())
		printedOptimizedObject = AsmPrinter::format(*optimizedObject->code());
	else
		printedOptimizedObject = optimizedObject->toString();

	// Re-parse new code for compilability
	if (!parse(_stream, _linePrefix, _formatted, printedOptimizedObject))
	{
		util::AnsiColorized(_stream, _formatted, {util::formatting::BOLD, util::formatting::CYAN})
			<< _linePrefix << "Result after the optimiser:" << std::endl;
		printPrefixed(_stream, printedOptimizedObject, _linePrefix + "  ");
		return TestResult::FatalError;
	}

	m_obtainedResult = "step: " + m_optimizerStep + "\n\n" + printedOptimizedObject + "\n";

	return checkResult(_stream, _linePrefix, _formatted);
}

std::shared_ptr<Object> YulOptimizerTest::parse(
	std::ostream& _stream,
	std::string const& _linePrefix,
	bool const _formatted,
	std::string const& _source
)
{
	YulStack yulStack = parseYul(_source);
	if (yulStack.hasErrors())
	{
		printYulErrors(yulStack, _stream, _linePrefix, _formatted);
		return nullptr;
	}

	return yulStack.parserResult();
}
